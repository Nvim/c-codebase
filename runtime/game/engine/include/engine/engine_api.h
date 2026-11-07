#ifndef ENGINE_FUNCTIONS_H
#define ENGINE_FUNCTIONS_H

#include <engine/input.h>
#include <engine/render_interface.h>

#include <strings/string.h>

typedef struct engine_api {
	string (*get_version)(void);
	engine_input_state const* (*get_input_state)(void);
	camera_2d_handle (*camera_create)(camera_2d_props props);
	camera_2d_handle (*camera_get_default)(void);
	camera_2d_props (*camera_get)(camera_2d_handle handle);
	void (*camera_update)(camera_2d_handle handle, camera_2d_props props);
} engine_api;

#endif // !ENGINE_FUNCTIONS_H
