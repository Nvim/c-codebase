#include <core/log.h>
#include <engine/engine_api.h>
#include <engine/game_api.h>
#include <strings/string.h>

#include <stdbool.h>

string game_get_version(void) { return str("0.0.1"); }

static engine_api const* engine_ptr = NULL;

static bool game_init(engine_api const* engine_api) {
	LOG_DEBUG("initializing game..");
	engine_ptr = engine_api;
	string engine_version = engine_api->engine_get_version();
	LOG_DEBUG("game intialized with engine API version " STR_FMT,
			  STR_FMT_ARGS(engine_version));
	return true;
}

static bool game_update(double delta) {
	static double elapsed = 0.f;
	elapsed += delta;
	LOG_INFO("game_update: %f ms elapsed. (+%f ms)", elapsed, delta);
	return true;
}

static bool game_shutdown(void) {
	LOG_INFO("shutting down game");
	return true;
}

game_api game_get_functions(void) {
	return (game_api){
		.game_get_version = game_get_version,
		.game_init = game_init,
		.game_update = game_update,
		.game_shutdown = game_shutdown,
	};
}
