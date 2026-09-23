#include "renderer.h"

#include <engine/render_interface.h>

#include <filesystem/filesystem.h>

#include <string.h>

#define SHADER_ROOT "./"

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

typedef enum renderer_frame_state {
	FRAME_BEGUN,	   // begun, should render
	FRAME_SHOULD_SKIP, // begun, should skip
	FRAME_RENDERING,
	FRAME_ENDED,
} renderer_frame_state;

renderer_frame_state _frame_state = FRAME_ENDED;

renderer* renderer_init(arena_t* arena, arena_t* frame_arena, arena_t* scratch,
						gpu_api* gpu, gpu_image_extent logical_resolution) {
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
		add_color_target(&builder, gpu->config.target_image_format, false);
		pipeline = gpu_pipeline_build(gpu, &builder);
		ensure_handle(pipeline.handle);
	}
	gpu->destroy_shader(gpu, shader);

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
			return NULL;
		}
	}
	gpu_buffer uniform_buffer = {0};
	renderer_uniform_data* uniform_ptr = NULL;
	gpu_device_address uniform_addr = {0};
	{ // Uniform buffer
		gpu_buffer_ci ci = {0};
		{
			ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
			ci.size = sizeof(renderer_uniform_data); // TODO: from renderer CI
			ci.usage = GPU_BUFFER_USAGE_STORAGE;
		}

		uniform_buffer = gpu->create_buffer(gpu, &ci);
		if (!check_handle(uniform_buffer.handle)) {
			LOG_ERR("couldn't create uniform buffer");
			return NULL;
		}

		// Map:
		gpu_buffer_map_result m = gpu->buffer_get_mapped(gpu, uniform_buffer);
		if (!m.ok) {
			LOG_ERR("couldn't map uniform buffer");
			return NULL;
		}
		uniform_ptr = m.ptr;

		// Addr:
		gpu_buffer_device_address_result a =
			gpu->buffer_get_device_address(gpu, uniform_buffer);
		if (!a.ok) {
			LOG_ERR("couldn't get uniform buffer gpu address");
			return NULL;
		}
		uniform_addr = a.address;
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
								  gpu->config.frames_in_flight, true)) {
			LOG_ERR("couldn't init draw buffers");
			return NULL;
		}
	}

	buffered_buffer instances_buffer = {0};
	{ // Draw items buffer
		gpu_buffer_ci ci = {0};
		{
			ci.memory_preference =
				uma ? GPU_BUFFER_MEMORY_MAPPED : GPU_BUFFER_MEMORY_GPU;
			ci.size = 8 * MEMORY_MIB; // TODO: from renderer CI
			ci.usage = GPU_BUFFER_USAGE_STORAGE;
		}
		if (!buffered_buffer_init(&instances_buffer, gpu, &ci,
								  gpu->config.frames_in_flight, true)) {
			LOG_ERR("couldn't init quad instances buffers");
			return NULL;
		}
	}

	renderer_camera camera = {0};
	{
		camera.world_position = (vec2){{0.f, 0.f}};
		camera.clip_from_view = mat_projection_ortho_centered(320, 180);

		// updates view matrix:
		renderer_camera_translate_by(&camera, (vec2){0});
	}

	{
		ret->arena = arena;
		ret->frame_arena = frame_arena;
		ret->gpu = gpu;
		ret->pipeline = pipeline;
		ret->staging_buffer = staging_buffer;
		ret->quad_instances_buffer = instances_buffer;
		ret->uniform_buffer = uniform_buffer;
		ret->uniform_ptr = uniform_ptr;
		ret->uniform_addr = uniform_addr;
		ret->draw_buffer = draw_buffer;
		ret->use_uma = uma;
		ret->draw_count = 0;
		ret->camera = camera;
		ret->logical_resolution = logical_resolution;
	}
	return ret;
}

bool renderer_shutdown(renderer* renderer) {
	if (renderer->gpu) {
		gpu_api* gpu = renderer->gpu;
		if (_frame_state == FRAME_RENDERING) { renderer_end_pass(renderer); }
		if (_frame_state == FRAME_BEGUN) { renderer_end_frame(renderer); }
		gpu->wait_idle(gpu);
		gpu->destroy_pipeline(gpu, renderer->pipeline);
		gpu->destroy_buffer(gpu, renderer->staging_buffer);
		gpu->destroy_buffer(gpu, renderer->uniform_buffer);
		buffered_buffer_destroy(&renderer->draw_buffer, gpu);
		buffered_buffer_destroy(&renderer->quad_instances_buffer, gpu);
	}

	return true;
}

bool renderer_begin_frame(renderer* renderer) {
	if (!renderer || !renderer->frame_arena) { return false; }
	if (_frame_state != FRAME_ENDED) {
		LOG_ERR("a frame is already in flight");
		return false;
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
		LOG_DEBUG("clamped viewport height: %ux%u", ret.width, ret.height);
	} else if (target_aspect < frame_aspect) {
		// Framebuffer wider than rendered image.
		// Black bars left & right
		ret.height = frame_ext.height;
		ret.width = frame_ext.height * target_aspect;
		LOG_DEBUG("clamped viewport width: %ux%u", ret.width, ret.height);
	}

	ret.x = (frame_ext.width - ret.width) / 2;
	ret.y = (frame_ext.height - ret.height) / 2;
	return ret;
}

bool renderer_begin_pass(renderer* renderer) {
	if (!renderer || _frame_state != FRAME_BEGUN) {
		LOG_ERR("can't render: invalid frame state %d", _frame_state);
		return false;
	}

	gpu_api* gpu = renderer->gpu;
	gpu_frame* frame = renderer->cur_frame;
	gpu_command_buffer cmd = frame->frame_cmd;

	check_handle(frame->framebuffer.handle);
	check_handle(cmd.handle);

	gpu_color_target color_target = {
		.clear_color = (gpu_color){.floats = {.1f, .1f, .1f, 1.f}},
		.load_op = GPU_LOAD_OP_CLEAR,
		.store_op = GPU_STORE_OP_STORE,
		.target = frame->framebuffer_view,
	};
	gpu_render_state render_state = {
		.color_target_count = 1,
		.color_targets = {color_target},
	};

	gpu->cmd_transition_image(gpu, cmd, frame->framebuffer,
							  GPU_IMAGE_STATE_RENDER_TARGET);

	gpu_viewport viewport = _compute_viewport(renderer);
	gpu->cmd_set_viewport(gpu, cmd, viewport);
	gpu->cmd_reset_scissor(gpu, cmd);
	gpu->cmd_begin_rendering(gpu, cmd, &render_state);

	renderer->camera.clip_from_view =
		mat_projection_ortho_centered(viewport.width, viewport.height);

	// Uniforms:
	renderer_uniform_data* u = renderer->uniform_ptr;
	u->mat_clip_from_world = renderer_camera_get_viewproj(&renderer->camera);
	u->draw_items = buffered_buffer_get_addr(&renderer->quad_instances_buffer);
	u->scale = viewport.width / (float)renderer->logical_resolution.width;

	_frame_state = FRAME_RENDERING;
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

	// Add indirect draw calls to buffer:
	gpu_indirect_draw_command draw = {0};
	{
		draw.first_instance = 0;
		draw.first_vertex = 0;
		draw.vertex_count = 6 * renderer->draw_count;
		draw.instance_count = renderer->draw_count;
	}

	// TODO: support staging
	gpu_indirect_draw_command* draws_ptr =
		buffered_buffer_get_mapped(&renderer->draw_buffer);
	if (!draws_ptr) {
		LOG_ERR("couldn't get indirect buffer pointer");
		return false;
	}
	draws_ptr[0] = draw;

	gpu->cmd_bind_pipeline(gpu, cmd, renderer->pipeline,
						   &(gpu_per_frame_data){
							   .frame_buffer_address = renderer->uniform_addr,
						   });

	gpu->cmd_draw_indirect(gpu, cmd,
						   buffered_buffer_get(&renderer->draw_buffer), 0, 1,
						   sizeof(gpu_indirect_draw_command));

	gpu->cmd_end_rendering(gpu, cmd);

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

	renderer->draw_count = 0;
	_frame_state = FRAME_ENDED;
	return ret;
}

void renderer_draw_quad(renderer* renderer, vec2 position, UNUSED vec3 color) {
	if (!renderer || _frame_state != FRAME_RENDERING) {
		LOG_ERR("invalid frame state %d", _frame_state);
		return;
	}

	gpu_frame* frame = renderer->cur_frame;

	// TODO: support staging upload
	renderer_draw_item* items =
		buffered_buffer_get_mapped(&renderer->quad_instances_buffer);
	if (!items) {
		LOG_ERR("couldn't get draw items for frame %u", frame->frame_index);
		return;
	}

	items[renderer->draw_count++] = (renderer_draw_item){
		.color = {.r = color.r, .g = color.g, .b = color.b, .a = 1.f},
		.position = {{position.x, position.y}},
		.scale = 1.f,
	};
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

/* ********************************[ CAMERA ]******************************** */

mat4 renderer_camera_get_view(renderer_camera* camera) {
	return camera->view_from_world;
}

mat4 renderer_camera_get_viewproj(renderer_camera* camera) {
	return mat4_mul(camera->clip_from_view, camera->view_from_world);
}

void renderer_camera_translate_by(renderer_camera* camera, vec2 offset) {

	camera->world_position.x += offset.x;
	camera->world_position.y += offset.y;

	camera->view_from_world = mat_translate_3d((vec3){
		{camera->world_position.x, camera->world_position.y, 0.f},
	});
}
