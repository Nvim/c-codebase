#include "engine.h"

#include <engine/engine_api.h>
#include <engine/game_api.h>
#include <engine/input.h>

#include <dlfcn.h>
#include <stdlib.h>

typedef void (*PFN_game_get_functions)(game_api* out);

typedef struct engine_state {
	bool init;
	bool running;
	uint64_t current_frame;

	bool game_loaded;
	void* game_dll_handle;
	game_api game_funcs;

	engine_api engine_funcs;
} engine_state;

static engine_state _state = {0};

static string _engine_get_version(void) { return str("0.0.1"); }

static PFN_game_get_functions _resolve_game_functions(void* dll_handle) {
	PFN_game_get_functions get_functions = NULL;
	char* error;
	dlerror();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
	get_functions =
		(PFN_game_get_functions)dlsym(dll_handle, "game_get_functions");
#pragma GCC diagnostic pop

	if ((error = dlerror()) != NULL) {
		LOG_ERR("couldn't load game DLL functions: %s", error);
		return NULL;
	}

	return get_functions;
}

static bool _load_game_dll(void** handle, game_api* out) {
	*handle = dlopen("./libgame.so", RTLD_LAZY);
	if (!*handle) {
		LOG_ERR("couldn't load game DLL: %s", dlerror());
		return false;
	}

	PFN_game_get_functions get_functions = _resolve_game_functions(*handle);
	if (get_functions == NULL) {
		LOG_ERR("couldn't load game DLL functions");
		return false;
	}

	get_functions(out);
	return true;
}

static bool _unload_game_dll(void* handle) {
	if (dlclose(handle) < 0) {
		LOG_ERR("couldn't close game DLL: %s", dlerror());
		return false;
	}
	return true;
}

bool engine_init(void) {

	_state.engine_funcs = (engine_api){
		.get_version = _engine_get_version,
		.get_input_state = engine_input_get_state,
	};

	if (!_load_game_dll(&_state.game_dll_handle, &_state.game_funcs)) {
		return false;
	}

	LOG_INFO("Game library v" STR_FMT " loaded",
			 STR_FMT_ARGS(_state.game_funcs.game_get_version()));

	_state.init = true;
	_state.game_loaded = true;

	return true;
}

bool engine_run(void) {
	if (!_state.init || !_state.game_loaded) { return false; }

	// Some errors don't exit immediately, but we shouldn't return true in the
	// end.
	bool exit_status = true;

	game_api const game_funcs = _state.game_funcs;
	engine_api const engine_funcs = _state.engine_funcs;

	// Init game
	if (!game_funcs.game_init(&engine_funcs)) { return false; }

	_state.running = true;

	// Step game a few frames
	// [TODO]: real game loop
	double time_ms = 0.f;
	double const delta_ms = 16.66f;
	while (time_ms < 10 * delta_ms) {

		if (!game_funcs.game_update(delta_ms)) {
			LOG_ERR("couldn't update game");
			exit_status = false;
			break;
		}

		time_ms += delta_ms;
		_state.current_frame++;
	}

	// Shutdown game
	if (!game_funcs.game_shutdown()) {
		LOG_ERR("couldn't shutdown game");
		return false;
	}

	return exit_status;
}

bool engine_shutdown(void) {
	if (!_state.init) { return true; }

	if (_state.game_loaded) {
		if (!_unload_game_dll(_state.game_dll_handle)) { return false; }
		_state.game_loaded = false;
	}

	_state.running = false;
	_state.init = false;

	return true;
}
