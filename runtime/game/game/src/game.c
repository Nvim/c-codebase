#include <core/log.h>
#include <engine/engine_api.h>
#include <engine/game_api.h>
#include <strings/string.h>

#include <stdbool.h>

string game_get_version(void) { return str("0.0.1"); }

static uint64_t _ticks = 0;
static UNUSED uint64_t _frames = 0;

static engine_api const* engine_ptr = NULL;

static bool game_init(engine_api const* engine_api) {
	LOG_DEBUG("initializing game..");
	engine_ptr = engine_api;
	UNUSED string engine_version = engine_api->get_version();
	LOG_DEBUG("game intialized with engine API version " STR_FMT,
			  STR_FMT_ARGS(engine_version));
	return true;
}

static void game_process_input(engine_input_state const* input) {
	if (input->pressed_keys[INPUT_KEY_K]) { LOG_DEBUG("game: key K pressed"); }
}

static bool game_update(double delta) {
	UNUSED static double elapsed = 0.f;
	elapsed += delta;
	if (_ticks > 100) {
		// LOG_WARN("game: 100 ticks passed. time to quit.");
	} else {
		// LOG_INFO("tick %llu: %f s elapsed. (+%f s)", _ticks++, elapsed,
		// delta);
	}
	return true;
}

static bool game_should_quit(void) { return false; }

static void game_draw(void) {
	// LOG_DEBUG("draw for frame %llu (%llu ticks)", _frames++, _ticks);
}

static bool game_shutdown(void) {
	LOG_INFO("shutting down game");
	return true;
}

void game_get_functions(game_api* out) {
	*out = (game_api){0};
	out->game_get_version = game_get_version;
	out->game_init = game_init;
	out->game_process_input = game_process_input;
	out->game_should_quit = game_should_quit;
	out->game_update = game_update;
	out->game_draw = game_draw;
	out->game_shutdown = game_shutdown;
}
