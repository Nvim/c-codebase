#ifndef ENGINE_FUNCTIONS_H
#define ENGINE_FUNCTIONS_H

#include <engine/input.h>
#include <engine/render_interface.h>

#include <strings/string.h>

typedef struct engine_api {
	string (*get_version)(void);
	engine_input_state const* (*get_input_state)(void);
	camera_2d_handle (*create_camera)(camera_2d_props props);
	camera_2d_props (*get_camera)(camera_2d_handle handle);
	void (*update_camera)(camera_2d_handle handle, camera_2d_props props);
} engine_api;

#endif // !ENGINE_FUNCTIONS_H
