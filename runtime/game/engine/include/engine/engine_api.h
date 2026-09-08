#ifndef ENGINE_FUNCTIONS_H
#define ENGINE_FUNCTIONS_H

#include <engine/input.h>

#include <strings/string.h>

typedef struct engine_api {
	string (*get_version)(void);
	engine_input_state const* (*get_input_state)(void);
} engine_api;

#endif // !ENGINE_FUNCTIONS_H
