#include <graphics/text_renderer.h>
#include <loaders/qoi_loader.h>
#include <string.h>

#define MAX_CHARS 8192

typedef struct buffered_buffer {
	gpu_buffer buffers[GPU_MAX_BUFFERING];
	gpu_device_address addresses[GPU_MAX_BUFFERING];
	void* maps[GPU_MAX_BUFFERING];
	uint8_t count;
	uint8_t current;
} buffered_buffer;

bool buffered_buffer_init(buffered_buffer* out, gpu_api* gpu, gpu_buffer_ci* ci,
						  uint8_t count, bool get_addresses) {
	if (!count || count > GPU_MAX_BUFFERING) { return false; }
	memset(out, 0, sizeof(*out));

	bool map = (ci->memory_preference & GPU_BUFFER_MEMORY_MAPPED);

	bool good = true;
	uint8_t i = 0;
	for (; i < count; i++) {
		gpu_buffer b = gpu->create_buffer(gpu, ci);
		if (!check_handle(b.handle)) {
			good = false;
			break;
		}

		if (get_addresses) {
			gpu_buffer_device_address_result ret =
				gpu->buffer_get_device_address(gpu, b);
			if (!ret.ok) {
				good = false;
				break;
			}
			out->addresses[i] = ret.address;
		}

		if (map) {
			gpu_buffer_map_result mret = gpu->buffer_get_mapped(gpu, b);
			if (!mret.ok) {
				good = false;
				break;
			}
			out->maps[i] = mret.ptr;
		}

		out->buffers[i] = b;
	}

	if (!good) {
		for (uint8_t j = 0; j < i; j++) {
			gpu->destroy_buffer(gpu, out->buffers[i]);
		}
		memset(out, 0, sizeof(*out));
		return false;
	}

	out->count = count;
	return true;
}

void buffered_buffer_advance(buffered_buffer* bufs) {
	bufs->current = (bufs->current + 1) % bufs->count;
}

gpu_buffer buffered_buffer_get(buffered_buffer* bufs) {
	return bufs->buffers[bufs->current];
}

gpu_device_address buffered_buffer_get_addr(buffered_buffer* bufs) {
	return bufs->addresses[bufs->current];
}

void* buffered_buffer_get_mapped(buffered_buffer* bufs) {
	return bufs->maps[bufs->current];
}

void buffered_buffer_destroy(buffered_buffer* bufs, gpu_api* gpu) {
	for (uint8_t i = 0; i < bufs->count; ++i) {
		gpu->destroy_buffer(gpu, bufs->buffers[i]);
	}
}

typedef struct text_renderer {
	gpu_api* gpu;
	gpu_image_extent atlas_size;
	font_atlas_info atlas_info;
	bool is_init;

	// GPU Resources:
	gpu_buffer staging;
	gpu_image atlas;
	gpu_pipeline pipeline;
	gpu_sampler sampler;
	gpu_image_view atlas_texture;
	uint32_t atlas_binding;
	uint32_t sampler_binding;
	buffered_buffer vertex_buffers;
	buffered_buffer uniform_buffers;

	// Current frame state:
	uint32_t vertex_count;
} text_renderer;

typedef struct vertex {
	float x;
	float y;
	float u;
	float v;
} vertex;

typedef struct uniform {
	gpu_device_address vertex_buffer;
	uint32_t image_index;
	uint32_t sampler_index;
} uniform;

text_renderer_t* text_renderer_init(gpu_api* gpu, arena_t* arena) {
	if (!arena || !gpu || !gpu->is_init) {
		LOG_ERR("text_renderer_init: invalid parameters");
		return NULL;
	}

	arena_checkpoint* checkpoint = arena_get_checkpoint(arena);
	text_renderer* ret = arena_push_z(arena, text_renderer);
	if (!ret) {
		LOG_ERR("text_renderer_init: couldn't allocate renderer");
		return NULL;
	}

	/* == Staging == */
	gpu_buffer_ci staging_ci = {0};
	{
		staging_ci.size = 32 * MEMORY_MIB;
		staging_ci.usage = GPU_BUFFER_USAGE_STAGING_UP;
	}

	gpu_buffer staging = gpu->create_buffer(gpu, &staging_ci);
	if (!check_handle(staging.handle)) {
		LOG_ERR("text_renderer_init: couldn't allocate staging buffer");
		arena_restore_checkpoint(arena, checkpoint, true);
		return NULL;
	}

	/* == Shaders == */
	gpu_shader vs = {0}, fs = {0};
	{ // Vertex shader
		fs_file_read_result res =
			fs_read_all_file(arena, str("build/shaders/text.slang.vert.spirv"));
		ASSERT(res.ok);
		gpu_shader_ci vs_ci = {0};
		vs_ci.type = GPU_SHADER_VERTEX;
		vs_ci.code = res.bytes;
		vs_ci.debug_name = str("text_vs");
		vs = gpu->create_shader(gpu, &vs_ci);
	}
	{ // Fragment shader
		fs_file_read_result res =
			fs_read_all_file(arena, str("build/shaders/text.slang.frag.spirv"));
		ASSERT(res.ok);
		gpu_shader_ci fs_ci = {0};
		fs_ci.type = GPU_SHADER_FRAGMENT;
		fs_ci.code = res.bytes;
		fs_ci.debug_name = str("text_fs");
		fs = gpu->create_shader(gpu, &fs_ci);
	}

	if (!check_handle(vs.handle) || !check_handle(fs.handle)) {
		LOG_ERR("text_renderer_init: couldn't create shaders");
		arena_restore_checkpoint(arena, checkpoint, true);
		return NULL;
	}

	/* == Pipeline == */
	gpu_pipeline pipeline;
	{
		gpu_pipeline_builder builder = {0};
		gpu_pipeline_builder_create(&builder);
		set_vertex_shader(&builder, vs);
		set_fragment_shader(&builder, fs);
		add_color_target(&builder, gpu->config.target_image_format, true);
		pipeline = gpu_pipeline_build(gpu, &builder);
	}
	gpu->destroy_shader(gpu, vs);
	gpu->destroy_shader(gpu, fs);
	if (!check_handle(pipeline.handle)) {
		LOG_ERR("text_renderer_init: couldn't build pipeline");
		arena_restore_checkpoint(arena, checkpoint, true);
		gpu->destroy_buffer(gpu, staging);
		return NULL;
	}

	/*  == Buffers == */
	// [TODO]: Support copy flow. This assumes shared memory
	gpu_buffer_ci indirect_ci = {0};
	{
		indirect_ci.usage = GPU_BUFFER_USAGE_INDIRECT;
		indirect_ci.size = MAX_CHARS * sizeof(gpu_indirect_draw_command) * 2;
		indirect_ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
	}
	gpu_buffer_ci vertex_ci = {0};
	{
		vertex_ci.usage = GPU_BUFFER_USAGE_STORAGE;
		vertex_ci.size = MAX_CHARS * sizeof(vertex) * 6; // 6 vertices per char
		vertex_ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
	}
	gpu_buffer_ci uniform_ci = {0};
	{
		uniform_ci.usage = GPU_BUFFER_USAGE_STORAGE;
		uniform_ci.size = sizeof(uniform);
		uniform_ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
	}

	if (!buffered_buffer_init(&ret->vertex_buffers, gpu, &vertex_ci,
							  gpu->config.frames_in_flight, true)) {
		LOG_ERR("text_renderer_init: couldn't create vertex buffers");
		arena_restore_checkpoint(arena, checkpoint, true);
		gpu->destroy_buffer(gpu, staging);
		return NULL;
	}

	if (!buffered_buffer_init(&ret->uniform_buffers, gpu, &uniform_ci,
							  gpu->config.frames_in_flight, true)) {
		LOG_ERR("text_renderer_init: couldn't create uniform buffers");
		arena_restore_checkpoint(arena, checkpoint, true);
		gpu->destroy_buffer(gpu, staging);
		buffered_buffer_destroy(&ret->vertex_buffers, gpu);
		return NULL;
	}

	/* == Sampler == */
	gpu_sampler_ci ci = {0};
	uint32_t sampler_index;
	gpu_sampler s = gpu->create_sampler(gpu, &ci, &sampler_index);
	if (!check_handle(s.handle)) {
		LOG_ERR("text_renderer_init: couldn't create sampler");
		arena_restore_checkpoint(arena, checkpoint, true);
		gpu->destroy_buffer(gpu, staging);
		buffered_buffer_destroy(&ret->vertex_buffers, gpu);
		buffered_buffer_destroy(&ret->uniform_buffers, gpu);
		return NULL;
	}

	ret->gpu = gpu;
	ret->staging = staging;
	ret->pipeline = pipeline;
	ret->sampler = s;
	ret->sampler_binding = sampler_index;
	ret->is_init = false;

	return (text_renderer_t*)ret;
}

// Create command buffer, upload image and submit
LIB_EXPORT bool text_renderer_load_atlas_from_file(text_renderer_t* renderer,
												   arena_t* scratch, path path,
												   font_atlas_info atlas_info) {
	if (!renderer || !path.len || !scratch) {
		LOG_ERR("text_renderer_load_atlas_from_file: invalid parameters");
		return false;
	}

	if (atlas_info.first_char >= atlas_info.last_char) {
		LOG_ERR("text_renderer_load_atlas_from_file: invalid atlas info");
		return false;
	}

	text_renderer* r = (text_renderer*)renderer;

	scratch_arena s;
	ASSERT(arena_get_scratch(scratch, &s));

	image_read_result img_read = load_qoi(scratch, s.arena, path, false);
	arena_clear_scratch(s);
	if (!img_read.ok) {
		LOG_ERR("text_renderer_load_atlas_from_file: couldn't atlas image");
		return false;
	}

	if (atlas_info.glyph_size.width >= img_read.width ||
		atlas_info.glyph_size.height >= img_read.height) {
		LOG_ERR("text_renderer_load_atlas_from_file: invalid glyph size");
		return false;
	}

	gpu_image_ci img_ci = {0};
	{
		img_ci.dimensions = (gpu_image_extent){.width = img_read.width,
											   .height = img_read.height};
		img_ci.format = IMAGE_FORMAT_R_UNORM;
		img_ci.type = GPU_IMAGE_TYPE_2D;
		img_ci.usage = GPU_IMAGE_USAGE_SAMPLED;
		img_ci.array_layers = 1;
		img_ci.mip_levels = 1;
	}

	gpu_image img = r->gpu->create_image(r->gpu, &img_ci);
	if (!check_handle(img.handle)) {
		LOG_ERR("text_renderer_load_atlas_from_file: coudn't create gpu image");
		return false;
	}

	// Create array with only R channel:
	uint64_t pixel_count = u8_array_size(img_read.bytes) / img_read.channels;
	u8_array pixel_data = u8_array_create(scratch, pixel_count);
	uint8_t* rgb = u8_array_begin(img_read.bytes);
	uint8_t* p = u8_array_begin(pixel_data);
	for (uint64_t i = 0; i < pixel_count; ++i) {
		p[i] = rgb[i * img_read.channels];
	}

	gpu_data_payload const img_data = {
		.data_format = GPU_DATA_ARRAY,
		.data.array = pixel_data,
	};

	gpu_image_data_payload img_payload = {
		.data = img_data,
		.height = img_read.width,
		.width = img_read.height,
	};

	if (!r->gpu->copy_to_staging_buffer(r->gpu, r->staging, img_payload.data)) {
		LOG_ERR("text_renderer_load_atlas_from_file: couldn't copy image to staging buffer"); // clang-format-ignore
		return false;
	}
	gpu_api gpu = *r->gpu;

	gpu_image_view view;
	uint32_t tex_bind;
	{ // Get image view
		gpu_image_view_ci view_ci = {0};
		{
			view_ci.image = img;
			view_ci.type = GPU_IMAGE_VIEW_TEXTURE;
			view_ci.base_layer = 0;
			view_ci.base_level = 0;
			view_ci.layer_count = GPU_COUNT_REMAINING;
			view_ci.level_count = GPU_COUNT_REMAINING;
		}
		view = gpu.create_image_view(&gpu, &view_ci);
		if (!gpu.get_image_view_shader_index(&gpu, view, &tex_bind)) {
			LOG_ERR("text_renderer_load_atlas_from_file: couldn't create texture"); // clang-format-ignore
			return false;
		}
	}

	{ // Upload image and wait
		gpu_command_buffer cmd = gpu.create_command_buffer(&gpu);
		ASSERT(check_handle(cmd.handle));
		gpu.cmd_begin_debug_marker(&gpu, cmd, "text renderer init");
		gpu.cmd_transition_image(&gpu, cmd, img, GPU_IMAGE_STATE_TRANSFER_DST);

		gpu_image_region_array regions =
			gpu_image_region_array_create(scratch, 1);
		ASSERT(regions.raw);
		gpu_image_region* reg = gpu_image_region_array_begin(regions);
		{
			reg->layer = 0;
			reg->level = 0;
		}

		gpu_image_copy_info img_copy_info = {
			.dst = img,
			.src = r->staging,
			.src_offset = 0,
			.regions = regions,
		};
		if (!gpu.cmd_copy_to_image(&gpu, cmd, &img_copy_info)) {
			LOG_ERR("text_renderer_load_atlas_from_file: couldn't upload image to gpu"); // clang-format-ignore
			gpu.destroy_command_buffer(&gpu, cmd);
			return false;
		}

		gpu.cmd_transition_image(&gpu, cmd, img, GPU_IMAGE_STATE_SHADER_READ);

		gpu.cmd_end_debug_marker(&gpu, cmd);

		gpu_fence fence = gpu.create_fence(&gpu, false);
		if (!gpu.submit_command_buffer_fence(&gpu, cmd, fence)) {
			LOG_ERR("text_renderer_load_atlas_from_file: couldn't submit transfer"); // clang-format-ignore
			gpu.destroy_command_buffer(&gpu, cmd);
			gpu.destroy_fence(&gpu, fence);
			return false;
		}
		if (!gpu.wait_fence(&gpu, fence, sec_to_ns(10))) {
			LOG_ERR("text_renderer_load_atlas_from_file: transfer timed-out");
			gpu.destroy_command_buffer(&gpu, cmd);
			gpu.destroy_fence(&gpu, fence);
			return false;
		}

		gpu.destroy_command_buffer(&gpu, cmd);
		gpu.destroy_fence(&gpu, fence);
	}

	{
		r->atlas_size = (gpu_image_extent){img_read.width, img_read.height};
		r->atlas_info = atlas_info;
		r->atlas = img;
		r->atlas_texture = view;
		r->atlas_binding = tex_bind;
		r->is_init = true;
	}

	return true;
}

void text_renderer_destroy(text_renderer_t* renderer) {
	text_renderer* r = (text_renderer*)renderer;

	gpu_api* gpu = r->gpu;
	gpu->destroy_buffer(gpu, r->staging);

	if (!r->is_init) { return; }

	gpu->destroy_image_view(gpu, r->atlas_texture);
	gpu->destroy_image(gpu, r->atlas);
	gpu->destroy_pipeline(gpu, r->pipeline);
	gpu->destroy_sampler(gpu, r->sampler);
	buffered_buffer_destroy(&r->vertex_buffers, gpu);
	buffered_buffer_destroy(&r->uniform_buffers, gpu);
}

bool text_renderer_add_text_pixels(text_renderer_t* renderer, string text,
								   uint32_t base_x, uint32_t base_y,
								   float scale) {
	text_renderer* r = (text_renderer*)renderer;

	if (scale < TEXT_MIN_SCALE || scale > TEXT_MAX_SCALE) {
		LOG_ERR("text_renderer_add_text: invalid scale value: %f", scale);
		return false;
	}
	gpu_api* gpu = r->gpu;

	gpu_image_extent framebuffer_size = gpu->get_framebuffer_extent(gpu);
	font_atlas_info info = r->atlas_info;

	uint32_t char_width = info.glyph_size.width + (info.pad_x * 2);
	uint32_t char_height = info.glyph_size.height + (info.pad_y * 2);

	uint32_t cols = r->atlas_size.width / char_width;
	vertex* verts = buffered_buffer_get_mapped(&r->vertex_buffers);

	uint32_t x = base_x;
	uint32_t y = base_y;

	char c;
	for (uint64_t i = 0; i < text.len; ++i) {
		c = text.str[i];

		if (c == '\n') {
			x = base_x;
			y = base_y + info.glyph_size.height * scale;
			continue;
		} else if (c < info.first_char || c > info.last_char) {
			LOG_WARN("text_renderer_add_text: char isn't in atlas: %c (%u)", c, c); // clang-format-ignore
			continue;
		}

		c = c - info.first_char;

		// Coords in atlas:
		uint32_t line = c / cols;
		uint32_t col = c % cols;

		uint32_t u0u = col * char_width + info.pad_x;
		uint32_t v0u = line * char_height + info.pad_y;
		uint32_t u1u = u0u + char_width;
		uint32_t v1u = v0u + char_height;
		// LOG_DEBUG("%c (%u -> %u): ATLAS: {%u, %u}, UV: {%u, %u}", c, c, cc,
		// col, line, u0u, v0u);
		float u0 = u0u / (float)r->atlas_size.width;
		float v0 = v0u / (float)r->atlas_size.height;
		float u1 = u1u / (float)r->atlas_size.width;
		float v1 = v1u / (float)r->atlas_size.height;

		float x0_ndc = (x / (float)framebuffer_size.width) * 2.f - 1.f;
		float y0_ndc = (y / (float)framebuffer_size.height) * 2.f - 1.f;

		float x_off = (char_width / (float)framebuffer_size.width) * scale;
		float y_off = (char_height / (float)framebuffer_size.height) * scale;
		float x1_ndc = x0_ndc + x_off;
		float y1_ndc = y0_ndc + y_off;

		x += info.glyph_size.width * scale;

		// clang-format off
		verts[r->vertex_count++] = (vertex){x0_ndc, y0_ndc, u0, v0}; // top left
		verts[r->vertex_count++] = (vertex){x0_ndc, y1_ndc, u0, v1}; // bot left
		verts[r->vertex_count++] = (vertex){x1_ndc, y1_ndc, u1, v1}; // bot right
		verts[r->vertex_count++] = (vertex){x0_ndc, y0_ndc, u0, v0}; // top left
		verts[r->vertex_count++] = (vertex){x1_ndc, y1_ndc, u1, v1}; // bot right
		verts[r->vertex_count++] = (vertex){x1_ndc, y0_ndc, u1, v0}; // top right
		// clang-format on
	}

	return true;
}

LIB_EXPORT bool text_renderer_add_text(text_renderer_t* renderer, string text,
									   float x, float y, float scale) {
	text_renderer* r = (text_renderer*)renderer;

	if (x >= 100.f || x < 0.f || y >= 100.f || y < 0.f) {
		LOG_ERR("text_renderer_add_text: invalid coordinates");
		return false;
	}

	gpu_api* gpu = r->gpu;
	gpu_image_extent framebuffer_size = gpu->get_framebuffer_extent(gpu);

	uint32_t xx = (x / 100.f) * framebuffer_size.width;
	uint32_t yy = (y / 100.f) * framebuffer_size.height;

	return text_renderer_add_text_pixels(renderer, text, xx, yy, scale);
}

static void _advance_buffers(text_renderer* r) {
	buffered_buffer_advance(&r->vertex_buffers);
	buffered_buffer_advance(&r->uniform_buffers);
}

LIB_EXPORT bool text_renderer_draw(text_renderer_t* renderer,
								   gpu_command_buffer cmd,
								   gpu_image_view target) {
	text_renderer* r = (text_renderer*)renderer;
	gpu_api* gpu = r->gpu;

	gpu_color_target color_target = {
		.clear_color = (gpu_color){.floats = {.1f, .1f, .1f, 1.f}},
		.load_op = GPU_LOAD_OP_LOAD,
		.store_op = GPU_STORE_OP_STORE,
		.target = target,
	};
	gpu_render_state render_state = {
		.color_target_count = 1,
		.color_targets = {color_target},
	};

	gpu_image target_img = gpu->get_image_from_view(gpu, target);
	if (!check_handle(target_img.handle)) {
		LOG_ERR("text_renderer_draw: couldn't get target's image");
		return false;
	}

	gpu->cmd_begin_debug_marker(gpu, cmd, "text renderer");
	gpu->cmd_transition_image(gpu, cmd, target_img,
							  GPU_IMAGE_STATE_RENDER_TARGET);

	gpu_device_address verts = buffered_buffer_get_addr(&r->vertex_buffers);
	gpu_device_address u = buffered_buffer_get_addr(&r->uniform_buffers);
	uniform* frame_data = buffered_buffer_get_mapped(&r->uniform_buffers);

	frame_data->image_index = r->atlas_binding;
	frame_data->sampler_index = r->sampler_binding;
	frame_data->vertex_buffer = verts;

	gpu_per_frame_data pc = {.frame_buffer_address = u};
	gpu->cmd_bind_pipeline(gpu, cmd, r->pipeline, &pc);
	gpu->cmd_reset_viewport(gpu, cmd);
	gpu->cmd_reset_scissor(gpu, cmd);
	gpu->cmd_begin_rendering(gpu, cmd, &render_state);

	gpu->cmd_draw(gpu, cmd, r->vertex_count, 1, 0, 0);
	gpu->cmd_end_rendering(gpu, cmd);

	gpu->cmd_end_debug_marker(gpu, cmd);

	_advance_buffers(r);
	r->vertex_count = 0;
	return true;
}
