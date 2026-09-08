#include <engine/engine_api.h>
#include <engine/game_api.h>
#include <engine/input.h>

#include <dlfcn.h>
#include <stdlib.h>

typedef game_api (*PFN_game_get_functions)(engine_api const*);

static string engine_get_version(void) { return str("0.0.1"); }

static engine_api const engine_funcs = {
	.get_version = engine_get_version,
	.get_input_state = engine_input_get_state,
};

static PFN_game_get_functions resolve_game_functions(void* dll_handle) {
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

int main(void) {
	void* handle;

	handle = dlopen("./libgame.so", RTLD_LAZY);
	if (!handle) {
		LOG_FATAL("couldn't load game DLL: %s", dlerror());
		return 1;
	}

	PFN_game_get_functions get_functions = resolve_game_functions(handle);
	if (get_functions == NULL) {
		LOG_FATAL("couldn't load game DLL functions");
		return 1;
	}

	game_api functions = get_functions(&engine_funcs);
	LOG_INFO("Game library v" STR_FMT " loaded",
			 STR_FMT_ARGS(functions.game_get_version()));

	if (!functions.game_init(&engine_funcs)) {
		LOG_FATAL("couldn't init game");
		return 1;
	}

	if (!functions.game_update(0.02f)) {
		LOG_FATAL("couldn't update game");
		return 1;
	}

	if (!functions.game_shutdown()) {
		LOG_FATAL("couldn't shutdown game");
		return 1;
	}

	if (dlclose(handle) < 0) {
		LOG_FATAL("couldn't close game DLL: %s", dlerror());
		return 1;
	}

	return 0;
}
