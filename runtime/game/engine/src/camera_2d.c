#include "camera_2d.h"

static void _camera_2d_update_matrices(renderer* renderer, camera_2d_props* cam,
									   uint32_t index) {
	ASSERT(index < RENDERER_MAX_CAMERAS);
	vec2* pos = &cam->position;
	renderer->camera_matrices[index].view_from_world =
		mat_translate_3d((vec3){{-pos->x, -pos->y, 0.f}});
	renderer->camera_matrices[index].clip_from_view =
		mat_projection_ortho_centered(cam->dimensions.width,
									  cam->dimensions.height);
}

camera_2d_handle camera_2d_create(renderer* renderer, camera_2d_props props) {

	camera_2d_props cam = props;
	camera_2d_pool_handle h = camera_2d_pool_new(renderer->cameras, &cam);
	uint32_t index = pool_handle_index(h.raw_handle);

	// Create props in pool
	if (!ensure_handle(h.raw_handle)) {
		LOG_ERR("couldn't allocate camera in pool");
		return (camera_2d_handle){POOL_INVALID_HANDLE};
	}

	// Compute matrices
	_camera_2d_update_matrices(renderer, &cam, index);

	return (camera_2d_handle){.handle = camera_2d_pool_unwrap(h)};
}

void camera_2d_update(renderer* renderer, camera_2d_handle handle,
					  camera_2d_props props) {
	if (!ensure_handle(handle.handle)) { return; }

	camera_2d_pool_handle h = camera_2d_pool_wrap(handle.handle);
	uint32_t index = pool_handle_index(handle.handle);
	camera_2d_props* cam = camera_2d_pool_get(renderer->cameras, h);
	if (!cam) {
		LOG_ERR("invalid camera handle");
		return;
	}

	// Update props in pool
	*cam = props;

	// Update matrices
	_camera_2d_update_matrices(renderer, cam, index);

	return;
}

camera_2d_props camera_2d_get_props(renderer* renderer,
									camera_2d_handle handle) {
	if (!ensure_handle(handle.handle)) { return (camera_2d_props){0}; }

	camera_2d_pool_handle h = camera_2d_pool_wrap(handle.handle);
	camera_2d_props* c = camera_2d_pool_get(renderer->cameras, h);

	return *c;
}

// mat4 camera_2d_get_viewproj(camera_2d* camera) {
// 	return mat4_mul(camera->matrices.clip_from_view,
// 					camera->matrices.view_from_world);
// }
