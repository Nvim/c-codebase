#include "renderer.h"

#include "camera_2d.h"
#include <engine/render_interface.h>

#include <filesystem/filesystem.h>
#include <loaders/qoi_loader.h>

#include <string.h>

#define SHADER_ROOT "./shaders/"
#define FONT_ATLAS_PATH "./engine_assets/oldschool.qoi"

typedef enum renderer_frame_state {
	FRAME_BEGUN,	   // begun, should render
	FRAME_SHOULD_SKIP, // begun, should skip
	FRAME_RENDERING,
	FRAME_ENDED,
} renderer_frame_state;

renderer_frame_state _frame_state = FRAME_ENDED;

static int const TEX_IDX_INVALID = -1;

static vec4 _color_to_vec4(gpu_color* col) {
	return (vec4){.r = col->r, .g = col->g, .b = col->b, .a = col->a};
}

static void _convert_draw_items(renderer_draw_item* src,
								internal_draw_item* dst) {
	*dst = (internal_draw_item){0};
	dst->pos = (vec2){{src->position.x, src->position.y}};
	dst->size = src->scale;
	dst->color = _color_to_vec4(&src->color);
	dst->texture_index = TEX_IDX_INVALID;

	// dst->texture_index = atlas_bind;
	// dst->uv1 = (vec2){{1.f, 1.f}};
	// TODO: texture for regular draws
}

renderer* renderer_init(arena_t* arena, arena_t* frame_arena, arena_t* scratch,
						gpu_api* gpu, gpu_image_extent logical_resolution) {
	if (!arena || !frame_arena || !gpu || !gpu->is_init) {
		LOG_ERR("invalid parameters");
		return NULL;
	}

	renderer* ret = arena_push_z(arena, renderer);
	if (!ret) {
		LOG_ERR("couldn't allocate renderer");
		return NULL;
	}

	bool uma = false;
	{ // Detect host visible unified memory
		gpu_memory_info mem_info = gpu->get_memory_info(gpu);
		if (mem_info.device_memory < 1 * MEMORY_GIB) {
			LOG_ERR("not enough gpu memory");
			return NULL;
		}
		if (mem_info.ratio > 0.8) { uma = true; }
	}

	gpu_shader shader = {0};
	{ // Shader
		fs_file_read_result res =
			fs_read_all_file(scratch, str(SHADER_ROOT "colored_quad.spv"));
		ASSERT(res.ok);
		gpu_shader_ci ci = {0};
		ci.type = GPU_SHADER_VERTEX_AND_FRAGMENT;
		ci.code = res.bytes;
		ci.debug_name = str("shader");
		shader = gpu->create_shader(gpu, &ci);
		ensure_handle(shader.handle);
	}

	gpu_pipeline pipeline = {0};
	{ // Pipeline
		gpu_pipeline_builder builder = {0};
		gpu_pipeline_builder_create(&builder);
		set_vertex_shader(&builder, shader);
		set_fragment_shader(&builder, shader);
		add_color_target(&builder, gpu->config.target_image_format, true);
		pipeline = gpu_pipeline_build(gpu, &builder);
		ensure_handle(pipeline.handle);
	}
	gpu->destroy_shader(gpu, shader);

	transfer_queue* transfer_queue =
		transfer_queue_init(arena, gpu, 128 * MEMORY_MIB, 9);
	if (!transfer_queue) {
		LOG_ERR("couldn't init transfer queue");
		return false;
	}

	gpu_buffer quad_index_buffer = {0};
	{ // Index buffer
		gpu_buffer_ci ci = {0};
		{
			ci.size = 6 * sizeof(uint32_t);
			ci.usage = GPU_BUFFER_USAGE_INDEX;
		}

		quad_index_buffer = gpu->create_buffer(gpu, &ci);
		if (!check_handle(quad_index_buffer.handle)) {
			LOG_ERR("couldn't create quad index buffer");
			return NULL;
		}
	}

	buffered_buffer uniform_buffer = {0};
	{ // Uniform buffer
		gpu_buffer_ci ci = {0};
		{
			ci.memory_preference =
				GPU_BUFFER_MEMORY_PREFER_GPU; // fine if it ends up in CPU
			ci.map = true;					  // uniform is always mapped
			ci.size = sizeof(renderer_uniform_data) * RENDERER_MAX_PASSES;
			ci.usage = GPU_BUFFER_USAGE_STORAGE;
		}

		if (!buffered_buffer_init(&uniform_buffer, gpu, &ci,
								  gpu->config.frames_in_flight, true)) {
			LOG_ERR("couldn't init uniform buffers");
			return NULL;
		}
	}

	buffered_buffer draw_buffer = {0};
	{ // Draw buffer
		gpu_buffer_ci ci = {0};
		{
			ci.memory_preference =
				GPU_BUFFER_MEMORY_MUST_GPU; // this one should be fast
			ci.map = uma; // upload with staging when no unified memory
			ci.size = sizeof(gpu_indirect_draw_command) * RENDERER_MAX_DRAWS;
			ci.usage = GPU_BUFFER_USAGE_INDIRECT;
		}
		if (!buffered_buffer_init(&draw_buffer, gpu, &ci,
								  gpu->config.frames_in_flight, true)) {
			LOG_ERR("couldn't init draw buffers");
			return NULL;
		}
	}

	buffered_buffer gpu_draw_items = {0};
	{ // Draw items buffer
		gpu_buffer_ci ci = {0};
		{
			ci.memory_preference = GPU_BUFFER_MEMORY_MUST_GPU;
			ci.map = uma; // upload with staging when no unified memory
			ci.size = sizeof(internal_draw_item) * RENDERER_MAX_DRAWS;
			ci.usage = GPU_BUFFER_USAGE_STORAGE;
		}
		if (!buffered_buffer_init(&gpu_draw_items, gpu, &ci,
								  gpu->config.frames_in_flight, true)) {
			LOG_ERR("couldn't init quad instances buffers");
			return NULL;
		}
	}

	camera_2d_pool cameras;
	camera_2d_props default_cam = {0};
	{
		cameras = camera_2d_pool_create(arena, RENDERER_MAX_CAMERAS);
		if (!cameras.raw) {
			LOG_ERR("couldn't create cameras pool");
			return NULL;
		}

		// [TODO][camera] game res usage
		default_cam.dimensions = (gpu_image_extent){320, 180};
		default_cam.position = (vec2){{0, 0}};
		default_cam.zoom = 1.f;
	}

	// Upload quad index buffer:
	{
		static uint32_t index_data[] = {0, 1, 2, 0, 2, 3};
		gpu_data_payload ibuf_payload = {0};
		{
			ibuf_payload.data_format = GPU_DATA_SPAN;
			ibuf_payload.data.span.data = index_data;
			ibuf_payload.data.span.size = sizeof(index_data);
			ibuf_payload.dst_offset = 0;
		}

		transfer_queue_upload up = {
			.type = TRQ_UPLOAD_BUFFER,
			.upload.buffer = {.dest = quad_index_buffer,
							  .acess = GPU_BUFFER_ACCESS_INDEX_READ},
			.payload = ibuf_payload,
		};
		if (!transfer_queue_push_upload(transfer_queue, up)) {
			LOG_ERR("couldn't enqueue indices upload");
			return NULL;
		}
	}

	internal_draw_item_array cpu_draw_items;
	{ // CPU draws array
		cpu_draw_items =
			internal_draw_item_array_create(arena, RENDERER_MAX_DRAWS);
		if (!cpu_draw_items.raw) {
			LOG_ERR("couldn't allocate draw items array");
			return NULL;
		}
	}

	gpu_image atlas_img = {0};
	gpu_image_view atlas_view = {0};
	gpu_sampler atlas_sampler = {0};
	uint32_t atlas_bind = 0;
	uint32_t atlas_sampler_bind = 0;
	font_atlas_info atlas_info = {0};
	gpu_image_extent atlas_size = {0};
	{ // font
		scratch_arena scratch2;
		arena_get_scratch(scratch, &scratch2);
		image_read_result img_read =
			load_qoi(scratch, scratch2.arena, str(FONT_ATLAS_PATH), false);
		if (!img_read.ok) {
			LOG_ERR("couldn't load font atlas image");
			return false;
		}
		arena_clear_scratch(scratch2);

		atlas_size = (gpu_image_extent){.width = img_read.width,
										.height = img_read.height};
		gpu_image_ci img_ci = {0};
		{
			img_ci.dimensions = atlas_size;
			img_ci.format = IMAGE_FORMAT_R_UNORM;
			img_ci.type = GPU_IMAGE_TYPE_2D;
			img_ci.usage = GPU_IMAGE_USAGE_SAMPLED;
			img_ci.array_layers = 1;
			img_ci.mip_levels = 1;
		}

		atlas_img = gpu->create_image(gpu, &img_ci);
		if (!check_handle(atlas_img.handle)) {
			LOG_ERR("coudn't create gpu image");
			return false;
		}
		// Create array with only R channel:
		uint64_t pixel_count =
			u8_array_size(img_read.bytes) / img_read.channels;
		u8_array pixel_data = u8_array_create(scratch, pixel_count);
		uint8_t* rgb = u8_array_begin(img_read.bytes);
		uint8_t* p = u8_array_begin(pixel_data);
		for (uint64_t i = 0; i < pixel_count; ++i) {
			p[i] = rgb[i * img_read.channels];
		}

		gpu_image_region_array regions =
			gpu_image_region_array_create(scratch, 1);
		ASSERT(regions.raw);
		gpu_image_region* reg = gpu_image_region_array_begin(regions);
		{
			reg->layer = 0;
			reg->level = 0;
		}

		transfer_queue_upload up = {
			.type = TRQ_UPLOAD_IMAGE,
			.upload.image = {.dest = atlas_img,
							 .usage = GPU_IMAGE_USAGE_SAMPLED,
							 .regions = regions},
			.payload =
				{
					.data_format = GPU_DATA_ARRAY,
					.data.array = pixel_data,
				},
		};

		if (!transfer_queue_push_upload(transfer_queue, up)) {
			LOG_ERR("couldn't enqueue font atlas upload");
			return NULL;
		}

		// Get image view
		gpu_image_view_ci view_ci = {0};
		{
			view_ci.image = atlas_img;
			view_ci.type = GPU_IMAGE_VIEW_TEXTURE;
			view_ci.base_layer = 0;
			view_ci.base_level = 0;
			view_ci.layer_count = GPU_COUNT_REMAINING;
			view_ci.level_count = GPU_COUNT_REMAINING;
		}
		atlas_view = gpu->create_image_view(gpu, &view_ci);
		if (!gpu->get_image_view_shader_index(gpu, atlas_view, &atlas_bind)) {
			LOG_ERR("couldn't create texture");
			return false;
		}

		// sampler
		gpu_sampler_ci ci = {0};
		{
			ci.address_mode_u = GPU_SAMPLER_ADRESS_CLAMP;
			ci.address_mode_v = GPU_SAMPLER_ADRESS_CLAMP;
		}
		atlas_sampler = gpu->create_sampler(gpu, &ci, &atlas_sampler_bind);
		if (!check_handle(atlas_sampler.handle)) {
			LOG_ERR("couldn't create font sampler");
		}

		{
			atlas_info.glyph_size = (vec2){{10, 14}};
			atlas_info.pad_x = 2;
			atlas_info.pad_y = 2;
			atlas_info.first_char = ' ';
			atlas_info.last_char = '~';
		}
	}

	if (!transfer_queue_batch_process(transfer_queue)) {
		LOG_ERR("couldn't start processing inital uploads");
		return NULL;
	}

	{
		ret->arena = arena;
		ret->frame_arena = frame_arena;
		ret->gpu = gpu;
		ret->pipeline = pipeline;
		ret->transfer_queue = transfer_queue;
		ret->quad_index_buffer = quad_index_buffer;
		ret->gpu_draw_items = gpu_draw_items;
		ret->uniform_buffer = uniform_buffer;
		ret->cpu_draw_items = cpu_draw_items;
		ret->draw_buffer = draw_buffer;
		ret->use_uma = uma;
		ret->draw_count = 0;
		ret->cameras = cameras;
		ret->logical_resolution = logical_resolution;

		ret->font_atlas = atlas_img;
		ret->font_atlas_texture = atlas_view;
		ret->font_sampler = atlas_sampler;
		ret->font_atlas_binding = atlas_bind;
		ret->font_sampler_binding = atlas_sampler_bind;
		ret->font_atlas_info = atlas_info;
		ret->font_atlas_size = atlas_size;
	}

	// Update default camera matrices
	camera_2d_pool_handle h = camera_2d_pool_new(cameras, &default_cam);
	if (!ensure_handle(h.raw_handle)) {
		LOG_ERR("couldn't allocate default camera in pool");
		return NULL;
	}
	camera_2d_update(ret, (camera_2d_handle){h.raw_handle}, default_cam);

	ret->default_camera_handle = (camera_2d_handle){h.raw_handle};

	return ret;
}

bool renderer_shutdown(renderer* renderer) {
	if (renderer->gpu) {
		gpu_api* gpu = renderer->gpu;
		if (_frame_state == FRAME_RENDERING) { renderer_end_pass(renderer); }
		if (_frame_state == FRAME_BEGUN) { renderer_end_frame(renderer); }
		transfer_queue_destroy(renderer->transfer_queue);
		gpu->wait_idle(gpu);
		gpu->destroy_pipeline(gpu, renderer->pipeline);
		gpu->destroy_buffer(gpu, renderer->quad_index_buffer);
		gpu->destroy_image(gpu, renderer->font_atlas);
		gpu->destroy_image_view(gpu, renderer->font_atlas_texture);
		gpu->destroy_sampler(gpu, renderer->font_sampler);
		buffered_buffer_destroy(&renderer->uniform_buffer, gpu);
		buffered_buffer_destroy(&renderer->draw_buffer, gpu);
		buffered_buffer_destroy(&renderer->gpu_draw_items, gpu);
	}

	return true;
}

bool renderer_begin_frame(renderer* renderer) {
	if (!renderer || !renderer->frame_arena) { return false; }
	if (_frame_state != FRAME_ENDED) {
		LOG_ERR("a frame is already in flight");
		return false;
	}

	// Tick transfer queue
	if (!transfer_queue_tick(renderer->transfer_queue)) {
		LOG_ERR("couldn't update transfer queue");
	}

	gpu_api* gpu = renderer->gpu;
	gpu_frame_result res = gpu->begin_frame(gpu);

	if (res.status == FRAME_ERROR) {
		LOG_ERR("couldn't begin frame");
		return false;
	}

	if (res.status == FRAME_SKIP) {
		_frame_state = FRAME_SHOULD_SKIP;
	} else {
		_frame_state = FRAME_BEGUN;
	}

	gpu_frame* frame = arena_push_z(renderer->frame_arena, gpu_frame);

	if (!frame) {
		LOG_ERR("couldn't allocate frame");
		return false;
	}

	memcpy(frame, &res.frame, sizeof(res.frame));
	renderer->cur_frame = frame;

	memset(&renderer->cur_pass, 0, sizeof(renderer->cur_pass));

	return true;
}

bool renderer_should_draw(renderer* renderer) {
	return renderer && _frame_state == FRAME_BEGUN;
}

static gpu_viewport _compute_viewport(renderer* renderer) {
	gpu_viewport ret = {0};

	gpu_api* gpu = renderer->gpu;
	gpu_image_extent frame_ext = gpu->get_framebuffer_extent(gpu);

	double frame_aspect = frame_ext.width / (double)frame_ext.height;
	double target_aspect = renderer->logical_resolution.width /
						   (double)renderer->logical_resolution.height;

	if (target_aspect == frame_aspect) {
		// happy path, simple scale is enough
		ret.width = frame_ext.width;
		ret.height = frame_ext.height;
	} else if (target_aspect > frame_aspect) {
		// Framebuffer is narrower than target image.
		// Black bars top & bottom
		ret.width = frame_ext.width;
		ret.height = frame_ext.width / target_aspect;
	} else if (target_aspect < frame_aspect) {
		// Framebuffer wider than rendered image.
		// Black bars left & right
		ret.height = frame_ext.height;
		ret.width = frame_ext.height * target_aspect;
	}

	ret.x = (frame_ext.width - ret.width) / 2;
	ret.y = (frame_ext.height - ret.height) / 2;
	renderer->scale = ret.width / (float)renderer->logical_resolution.width;
	return ret;
}

static renderer_uniform_data* _get_current_cpu_uniform(renderer* renderer) {
	renderer_uniform_data* u =
		buffered_buffer_get_mapped(&renderer->uniform_buffer);
	if (!u) {
		LOG_ERR("couldn't get uniform for frame %u", renderer->cur_frame->frame_index); // NOLINT
		return false;
	}
	u += renderer->cur_pass.index % RENDERER_MAX_PASSES;
	return u;
}

static gpu_device_address _get_current_gpu_uniform(renderer* renderer) {
	gpu_device_address addr =
		buffered_buffer_get_addr(&renderer->uniform_buffer);
	addr += sizeof(renderer_uniform_data) *
			(renderer->cur_pass.index % RENDERER_MAX_PASSES);
	return addr;
}

bool renderer_begin_pass(renderer* renderer, camera_2d_handle camera) {
	if (!renderer || _frame_state != FRAME_BEGUN) {
		LOG_ERR("can't render: invalid frame state %d", _frame_state);
		return false;
	}

	// Get things
	gpu_api* gpu = renderer->gpu;
	gpu_frame* frame = renderer->cur_frame;
	gpu_command_buffer cmd = frame->frame_cmd;

	check_handle(frame->framebuffer.handle);
	check_handle(cmd.handle);

	// Begin rendering
	gpu_color_target color_target = {
		.clear_color = (gpu_color){.floats = {.1f, .1f, .1f, 1.f}},
		.load_op = renderer->cur_pass.index == 0 ? GPU_LOAD_OP_CLEAR
												 : GPU_LOAD_OP_LOAD,
		.store_op = GPU_STORE_OP_STORE,
		.target = frame->framebuffer_view,
	};
	gpu_render_state render_state = {
		.color_target_count = 1,
		.color_targets = {color_target},
	};

	char pass_label[32];
	sprintf(pass_label, "PASS #%u", renderer->cur_pass.index);
	gpu->cmd_push_debug_label(gpu, cmd, pass_label);
	gpu->cmd_transition_image(gpu, cmd, frame->framebuffer,
							  GPU_IMAGE_STATE_RENDER_TARGET);

	gpu_viewport viewport = _compute_viewport(renderer);
	gpu->cmd_set_viewport(gpu, cmd, viewport);
	gpu->cmd_reset_scissor(gpu, cmd);
	gpu->cmd_begin_rendering(gpu, cmd, &render_state);

	// Update uniforms
	renderer_uniform_data* u = _get_current_cpu_uniform(renderer);

	uint32_t index = pool_handle_index(camera.handle);
	u->view_from_world = renderer->camera_matrices[index].view_from_world;
	u->clip_from_view = renderer->camera_matrices[index].clip_from_view;
	u->draw_items = buffered_buffer_get_addr(&renderer->gpu_draw_items);

	_frame_state = FRAME_RENDERING;

	// [NOTE]: pass index is incremented after ending, so 1st pass is #0
	// renderer->cur_pass.index++;

	return true;
}

bool renderer_end_pass(renderer* renderer) {
	if (!renderer || _frame_state != FRAME_RENDERING) {
		LOG_ERR("can't render: invalid frame state %d", _frame_state);
		return false;
	}

	gpu_api* gpu = renderer->gpu;
	gpu_frame* frame = renderer->cur_frame;
	gpu_command_buffer cmd = frame->frame_cmd;
	uint32_t pass_idx = renderer->cur_pass.index;
	uint32_t first_inst = renderer->cur_pass.first_instance;

	{ // Add indirect draw calls to buffer:
		gpu_cmd_indirect_indexed_draw_command draw = {0};
		{
			draw.first_instance = first_inst;
			draw.first_index = 0;
			draw.index_count = 6;
			draw.instance_count = renderer->cur_pass.draw_count;
			draw.vertex_offset = 0;
		}

		gpu_cmd_indirect_indexed_draw_command* draws_ptr =
			buffered_buffer_get_mapped(&renderer->draw_buffer);
		if (draws_ptr) {
			draws_ptr[pass_idx] = draw;
		} else {
			transfer_queue_upload up = {0};
			{
				up.type = TRQ_UPLOAD_BUFFER;
				up.upload.buffer.acess = GPU_BUFFER_ACCESS_INDIRECT_DRAW_READ;
				up.upload.buffer.dest =
					buffered_buffer_get(&renderer->draw_buffer);
				up.payload.data_format = GPU_DATA_SPAN;
				up.payload.data.span.data = &draw;
				up.payload.data.span.size = sizeof(draw);
				up.payload.dst_offset =
					pass_idx * (sizeof(draw)); // assumes 1 draw per pass
				/* [NOTE]: dst_offset could be 0 here. Since we flush
				 * transfer queue on each pass, barrier happens so it's safe
				 * to re-write in the same place. We use a different offset
				 * to match the UMA path, where we don't want to explicitly
				 * sync pass #1 needing to finish before pass #2.
				 */
			}
			transfer_queue_push_upload(renderer->transfer_queue, up);
		}
	}

	{ // Update instances buffer
		internal_draw_item* items =
			buffered_buffer_get_mapped(&renderer->gpu_draw_items);
		size_t items_size = renderer->cur_pass.draw_count * sizeof(*items);

		internal_draw_item* first_draw;
		internal_draw_item_array_at(renderer->cpu_draw_items, &first_draw,
									renderer->cur_pass.first_instance);
		if (!first_draw) { return false; }

		// Use UMA if available
		if (items) {
			memcpy(&items[first_inst], first_draw, items_size);
		} else {
			transfer_queue_upload up = {0};
			{
				up.type = TRQ_UPLOAD_BUFFER;
				up.upload.buffer.acess = GPU_BUFFER_ACCESS_VERTEX_SHADER_READ;
				up.upload.buffer.dest =
					buffered_buffer_get(&renderer->gpu_draw_items);
				up.payload.data_format = GPU_DATA_SPAN;
				up.payload.data.span.data = first_draw;
				up.payload.data.span.size = items_size;
				up.payload.dst_offset = first_inst * sizeof(*first_draw);
			}
			transfer_queue_push_upload(renderer->transfer_queue, up);
		}
	}

	// submit this pass's transfer tasks
	transfer_queue_batch_process(renderer->transfer_queue);

	gpu_device_address uniform_addr = _get_current_gpu_uniform(renderer);

	gpu->cmd_bind_pipeline(gpu, cmd, renderer->pipeline,
						   &(gpu_per_frame_data){
							   .frame_buffer_address = uniform_addr,
						   });
	gpu->cmd_bind_index_buffer(gpu, cmd, renderer->quad_index_buffer);

	uint64_t stride = sizeof(gpu_cmd_indirect_indexed_draw_command);
	gpu->cmd_draw_indirect_indexed(gpu, cmd,
								   buffered_buffer_get(&renderer->draw_buffer),
								   pass_idx * stride, // assumes 1 draw per pass
								   1, stride);

	gpu->cmd_end_rendering(gpu, cmd);

	{ // update pass info for next pass
		renderer->cur_pass.index++;
		renderer->cur_pass.first_instance += renderer->cur_pass.draw_count;
		renderer->cur_pass.draw_count = 0;
	}

	_frame_state = FRAME_BEGUN;
	return true;
}

bool renderer_end_frame(renderer* renderer) {
	if (!renderer ||
		(_frame_state != FRAME_BEGUN && _frame_state != FRAME_SHOULD_SKIP)) {
		LOG_ERR("can't render: invalid frame state %d", _frame_state);
		return false;
	}

	gpu_api* gpu = renderer->gpu;
	gpu_frame* frame = renderer->cur_frame;

	bool ret = gpu->end_frame(gpu, frame);

	buffered_buffer_advance(&renderer->uniform_buffer);
	buffered_buffer_advance(&renderer->gpu_draw_items);
	buffered_buffer_advance(&renderer->draw_buffer);

	renderer->draw_count = 0;
	_frame_state = FRAME_ENDED;
	memset(&renderer->cur_pass, 0, sizeof(renderer->cur_pass));

	return ret;
}

void renderer_draw_quad(renderer* renderer, vec2 position, vec3 color,
						vec2 scale) {
	if (!renderer || _frame_state != FRAME_RENDERING) {
		LOG_ERR("invalid frame state %d", _frame_state);
		return;
	}

	internal_draw_item draw_item = {
		.color = {.r = color.r, .g = color.g, .b = color.b, .a = 1.f},
		.pos = {{position.x, position.y}},
		.size = scale,
	};

	internal_draw_item* draw = NULL;
	internal_draw_item_array_at(renderer->cpu_draw_items, &draw,
								renderer->draw_count++);
	*draw = draw_item;
	renderer->cur_pass.draw_count++;
}

bool renderer_draw(renderer* renderer, renderer_draw_item_array draw_items,
				   uint64_t draw_count) {
	if (!renderer || _frame_state != FRAME_RENDERING) {
		LOG_ERR("invalid frame state %d", _frame_state);
		return false;
	}

	for (uint64_t i = 0; i < draw_count; ++i) {
		renderer_draw_item* external = NULL;
		renderer_draw_item_array_at(draw_items, &external,
									renderer->draw_count);

		internal_draw_item* internal = NULL;
		internal_draw_item_array_at(renderer->cpu_draw_items, &internal,
									renderer->draw_count);
		if (!internal || !external) {
			LOG_ERR("couldn't get draw_item #%u", i);
			return false;
		}

		_convert_draw_items(external, internal);
		renderer->cur_pass.draw_count++;
		renderer->draw_count++;
	}

	return true;
}

static bool _draw_text(renderer* renderer, renderer_ui_item* item) {
	font_atlas_info info = renderer->font_atlas_info;

	uint32_t char_width = info.glyph_size.x + (info.pad_x * 2);
	uint32_t char_height = info.glyph_size.y + (info.pad_y * 2);

	uint32_t cols = renderer->font_atlas_size.width / char_width;
	uint32_t x = item->position.x;
	uint32_t y = item->position.y;

	string text = item->msg;
	float scale = 1.f; // todo

	char c;
	for (uint64_t i = 0; i < text.len; ++i) {
		internal_draw_item* internal = NULL;
		internal_draw_item_array_at(renderer->cpu_draw_items, &internal,
									renderer->draw_count);
		if (!internal) {
			LOG_ERR("couldn't get ui_draw_item #%u", renderer->draw_count);
			return false;
		}
		c = text.str[i];

		if (c == '\n') {
			x = item->position.x;
			y = item->position.y -
				info.glyph_size.y * scale; // [TODO]: figure this out
			continue;
		} else if (c < info.first_char || c > info.last_char) {
				LOG_WARN("char isn't in atlas: %c (%u)", c, c); // clang-format-ignore
			continue;
		}

		c = c - info.first_char;

		// Coords in atlas:
		uint32_t line = c / cols;
		uint32_t col = c % cols;

		uint32_t u0u = col * char_width + info.pad_x;
		uint32_t v0u = line * char_height + info.pad_y;
		uint32_t u1u = u0u + char_width - info.pad_x;
		uint32_t v1u = v0u + char_height - info.pad_y;
		float u0 = u0u / (float)renderer->font_atlas_size.width;
		float v0 = v0u / (float)renderer->font_atlas_size.height;
		float u1 = u1u / (float)renderer->font_atlas_size.width;
		float v1 = v1u / (float)renderer->font_atlas_size.height;

		x += 10.f;

		internal->pos = (vec2){{x, y}};
		internal->texture_index = renderer->font_atlas_binding;
		internal->sampler_index = renderer->font_sampler_binding;
		internal->size = (vec2){{10.f, 10.f}};
		// TODO: figure out if all uvs need Y flipped or only this one
		internal->uv0 = (vec2){{u0, v1}};
		internal->uv1 = (vec2){{u1, v0}};

		renderer->draw_count++;
		renderer->cur_pass.draw_count++;
	}
	return true;
}

bool renderer_draw_ui(renderer* renderer, renderer_ui_item_array items,
					  uint64_t draw_count) {
	if (!renderer || _frame_state != FRAME_RENDERING) {
		LOG_ERR("invalid frame state %d", _frame_state);
		return false;
	}
	gpu_api* gpu = renderer->gpu;

	for (uint64_t i = 0; i < draw_count; ++i) {
		renderer_ui_item* external = NULL;
		renderer_ui_item_array_at(items, &external, i);

		if (!external) {
			LOG_ERR("couldn't get draw_item #%u", i);
			return false;
		}

		if (external->space == RENDERER_SPACE_UV) {
			// [TODO]: use the pass's render target instead
			gpu_image fb = renderer->cur_frame->framebuffer;
			UNUSED gpu_image_extent extent = gpu->get_image_extent(gpu, fb);

			// convert [0;1] to world space
		}

		_draw_text(renderer, external);
	}

	return true;
}

bool renderer_load_texture(UNUSED renderer* renderer,
						   UNUSED renderer_texture* out,
						   UNUSED renderer_texture_ci ci) {
	ASSERT(false);
	return false;
}

bool renderer_create_buffer(UNUSED renderer* renderer, UNUSED gpu_buffer* out,
							UNUSED renderer_buffer_ci ci) {
	ASSERT(false);
	return false;
}
