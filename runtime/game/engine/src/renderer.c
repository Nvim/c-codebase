#include "renderer.h"

#include <filesystem/filesystem.h>

#include <string.h>

#define SHADER_ROOT "../../../shaders/"

/* ***************************[ BUFFERED BUFFER ]**************************** */

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

/* *******************************[ RENDERER ]******************************* */
renderer* renderer_init(arena_t* arena, arena_t* frame_arena, arena_t* scratch,
						gpu_api* gpu) {
	if (!arena || !frame_arena || !gpu || !gpu->is_init) {
		LOG_ERR("invalid parameters");
		return NULL;
	}

	renderer* ret = arena_push(arena, renderer);
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

	gpu_shader vs = {0}, fs = {0};
	{ // Vertex shader
		fs_file_read_result res =
			fs_read_all_file(scratch, str(SHADER_ROOT "quad.slang.vert.spirv"));
		ASSERT(res.ok);
		gpu_shader_ci vs_ci = {0};
		vs_ci.type = GPU_SHADER_VERTEX;
		vs_ci.code = res.bytes;
		vs_ci.debug_name = str("quad_vs");
		vs = gpu->create_shader(gpu, &vs_ci);
		ASSERT(check_handle(vs.handle));
	}
	{ // Fragment shader
		fs_file_read_result res =
			fs_read_all_file(scratch, str(SHADER_ROOT "quad.slang.frag.spirv"));
		ASSERT(res.ok);
		gpu_shader_ci fs_ci = {0};
		fs_ci.type = GPU_SHADER_FRAGMENT;
		fs_ci.code = res.bytes;
		fs_ci.debug_name = str("quad_fs");
		fs = gpu->create_shader(gpu, &fs_ci);
		ASSERT(check_handle(fs.handle));
	}

	gpu_pipeline pipeline = {0};
	{ // Pipeline
		gpu_pipeline_builder builder = {0};
		gpu_pipeline_builder_create(&builder);
		set_vertex_shader(&builder, vs);
		set_fragment_shader(&builder, fs);
		add_color_target(&builder, gpu->config.target_image_format, false);
		pipeline = gpu_pipeline_build(gpu, &builder);
		ASSERT(check_handle(pipeline.handle));
	}

	gpu_buffer staging_buffer = {0};
	{ // Staging buffer
		gpu_buffer_ci ci = {0};
		{
			ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
			ci.size = 16 * MEMORY_MIB; // TODO: from renderer CI
			ci.usage = GPU_BUFFER_USAGE_STAGING_UP;
		}

		staging_buffer = gpu->create_buffer(gpu, &ci);
		if (!check_handle(staging_buffer.handle)) {
			LOG_ERR("couldn't create staging buffer");
		}
	}

	buffered_buffer draw_buffer = {0};
	{ // Draw buffer
		gpu_buffer_ci ci = {0};
		{
			ci.memory_preference =
				uma ? GPU_BUFFER_MEMORY_MAPPED : GPU_BUFFER_MEMORY_GPU;
			ci.size = 8 * MEMORY_MIB; // TODO: from renderer CI
			ci.usage = GPU_BUFFER_USAGE_INDIRECT;
		}
		if (!buffered_buffer_init(&draw_buffer, gpu, &ci,
								  gpu->config.frames_in_flight, uma)) {
			LOG_ERR("couldn't init draw buffers");
		}
	}

	gpu->destroy_shader(gpu, vs);
	gpu->destroy_shader(gpu, fs);

	{
		ret->arena = arena;
		ret->frame_arena = frame_arena;
		ret->gpu = gpu;
		ret->pipeline = pipeline;
		ret->staging_buffer = staging_buffer;
		ret->draw_buffer = draw_buffer;
		ret->use_uma = uma;
	}
	return ret;
}

bool renderer_shutdown(UNUSED renderer* renderer) { ASSERT(false); }

bool renderer_begin_frame(UNUSED renderer* renderer) { ASSERT(false); }

bool renderer_begin_pass(UNUSED renderer* renderer) { ASSERT(false); }

bool renderer_end_pass(UNUSED renderer* renderer) { ASSERT(false); }

bool renderer_end_frame(UNUSED renderer* renderer) { ASSERT(false); }

void renderer_draw_quad(UNUSED vec2 position, UNUSED vec3 color) {
	ASSERT(false);
}

bool renderer_load_texture(UNUSED renderer* renderer,
						   UNUSED renderer_texture* out,
						   UNUSED renderer_texture_ci ci) {
	ASSERT(false);
}

bool renderer_create_buffer(UNUSED renderer* renderer, UNUSED gpu_buffer* out,
							UNUSED renderer_buffer_ci ci) {
	ASSERT(false);
}

/* ********************************[ CAMERA ]******************************** */

mat4 renderer_camera_get_mvp(UNUSED renderer_camera* camera) { ASSERT(false); }

mat4 renderer_camera_get_mvp_a(UNUSED arena_t* arena,
							   UNUSED renderer_camera* camera) {
	ASSERT(false);
}

void renderer_camera_translate_by(UNUSED vec2 offset) { ASSERT(false); }
