#ifndef GAME_FUNCTIONS_H
#define GAME_FUNCTIONS_H

#include <engine/input.h>
#include <engine/render_interface.h>
#include <strings/string.h>

#include <stdbool.h>

typedef struct engine_api engine_api;

typedef void* game_state_ptr;

typedef struct game_api {
	string (*game_get_version)(void);
	bool (*game_init)(engine_api const* engine_api, arena_t* game_arena);
	void (*game_process_input)(engine_input_state const* input);
	bool (*game_update)(double delta);
	bool (*game_should_quit)(void);
	void (*game_draw)(renderer_packet* out);
	game_state_ptr (*game_before_reload)(void);
	bool (*game_reload)(game_state_ptr state);
	bool (*game_shutdown)(void);
} game_api;

LIB_EXPORT void game_get_functions(game_api* out);

#endif // !GAME_FUNCTIONS_H
