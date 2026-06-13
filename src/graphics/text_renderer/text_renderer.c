#include <graphics/text_renderer.h>
#include <loaders/qoi_loader.h>

typedef struct text_renderer {
	gpu_api* gpu;
	gpu_image_extent atlas_size;
	gpu_image_extent glyph_size;
	float scale;
	bool is_init;

	// GPU Resources:
	gpu_buffer staging;
	gpu_image atlas;
	gpu_image_view atlas_texture;
	uint32_t atlas_binding;
} text_renderer;

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

	// [TODO]: Load shaders and create pipeline

	ret->gpu = gpu;
	ret->staging = staging;
	ret->scale = 1.f;
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
		r->glyph_size = atlas_info.glyph_size;
		r->glyph_size = atlas_info.glyph_size;
		r->atlas = img;
		r->atlas_texture = view;
		r->atlas_binding = tex_bind;
		r->is_init = true;
	}

	return true;
}

void text_renderer_set_scale(text_renderer_t* renderer, float scale) {
	text_renderer* r = (text_renderer*)renderer;
	if (scale < 0.125f || scale > 5.f) {
		LOG_ERR("text_renderer_set_scale: invalid scale value: %f", scale);
		return;
	}

	r->scale = scale;
}

void text_renderer_destroy(text_renderer_t* renderer) {
	text_renderer* r = (text_renderer*)renderer;

	gpu_api* gpu = r->gpu;
	gpu->destroy_buffer(gpu, r->staging);

	if (!r->is_init) {
		// Nothing else to destroy for now. [TODO]: pipeline
		return;
	}

	gpu->destroy_image_view(gpu, r->atlas_texture);
	gpu->destroy_image(gpu, r->atlas);
}
