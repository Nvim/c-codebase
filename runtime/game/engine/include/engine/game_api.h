#ifndef GAME_FUNCTIONS_H
#define GAME_FUNCTIONS_H

typedef struct engine_api engine_api;

#include <stdbool.h>
#include <strings/string.h>

typedef struct game_api {
	string (*game_get_version)(void);
	bool (*game_init)(engine_api const* engine_api);
	bool (*game_update)(double delta);
	// TODO: game_draw
	// TODO: game_reload
	bool (*game_shutdown)(void);
} game_api;

game_api game_get_functions(void);

#endif // !GAME_FUNCTIONS_H
