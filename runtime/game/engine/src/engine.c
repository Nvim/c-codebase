#include "engine.h"

#include <engine/engine_api.h>
#include <engine/game_api.h>
#include <engine/input.h>

#include <allocators/arena.h>
#include <graphics/graphics.h>

#include <dlfcn.h>
#include <stdlib.h>

#define MAX_FRAMETIME_S 0.33

typedef void (*PFN_game_get_functions)(game_api* out);

typedef struct engine_timer {
	// double render_target_frametime_s; // from config
	double game_timestep_s; // [TODO]: from game init

	// runtime state
	uint64_t current_frame;
	double previous_time_s;
	double current_time_s;

	double frame_time_s;
	double frame_accumulator_s;
} engine_timer;

typedef struct engine_state {
	bool init;
	bool running;
	engine_timer timer;

	bool game_loaded;
	void* game_dll_handle;
	game_api game_funcs;

	engine_api engine_funcs;
	window_t* window;
	engine_config config;

	arena_t* engine_arena;
	arena_t* game_arena;

	gpu_api gpu;
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

static bool _create_arenas(void) {
	engine_config config = _state.config;
	arena_create_info arena_ci = {0};
	{
		arena_ci.initial_size =
			MIN(config.max_engine_memory_bytes, 1 * MEMORY_MIB);
		arena_ci.max_size = config.max_engine_memory_bytes;
		arena_ci.alignment = 8;
		arena_ci.flags = ARENA_GROWING;
	}
	_state.engine_arena = arena_create_from_config(arena_ci);

	{
		arena_ci.initial_size =
			MIN(config.max_game_memory_bytes, 1 * MEMORY_MIB);
		arena_ci.max_size = config.max_game_memory_bytes;
	}
	_state.game_arena = arena_create_from_config(arena_ci);

	if (!arena_pool_init()) {
		LOG_ERR("couldn't initialize arena pool");
		return false;
	}

	return _state.game_arena && _state.engine_arena;
}

static bool _load_game(void) {
	_state.engine_funcs = (engine_api){
		.get_version = _engine_get_version,
		.get_input_state = engine_input_get_state,
	};

	if (!_load_game_dll(&_state.game_dll_handle, &_state.game_funcs)) {
		return false;
	}

	LOG_INFO("Game library v" STR_FMT " loaded",
			 STR_FMT_ARGS(_state.game_funcs.game_get_version()));

	_state.game_loaded = true;
	return true;
}

static bool _init_gpu(void) {
	gpu_api_config gpu_ci = {0};
	{
		gpu_ci.app_name = "todo";
		gpu_ci.frames_in_flight = 3;
		gpu_ci.target_image_format = IMAGE_FORMAT_RGBA_SRGB;
		gpu_ci.type = GRAPHICS_API_VULKAN;
		gpu_ci.present_mode = GPU_PRESENT_MODE_VSYNC;
		// TODO: config limits
	}

	scratch_arena scratch = {0};
	if (!arena_get_scratch(NULL, &scratch)) { return false; }

	if (!gpu_create_api(gpu_ci, &_state.gpu)) { return false; }
	_state.gpu.initialize(&_state.gpu, _state.window, _state.engine_arena,
						  scratch.arena);

	arena_clear_scratch(scratch);

	return _state.gpu.is_init;
}

// [TODO]: destroy scratch arena pool as well
static bool _destroy_arenas(void) {
	if (!_state.init) { return false; }
	if (_state.running) {
		LOG_ERR("can't destroy arenas while game is running");
		return false;
	}

	if (_state.engine_arena) { arena_destroy(_state.engine_arena); }
	if (_state.game_arena) { arena_destroy(_state.game_arena); }

	_state.game_arena = NULL;
	_state.engine_arena = NULL;

	return true;
}

static void _destroy_platform(void) {
	if (_state.window) { platform_destroy_window(_state.window); }
	platform_destroy();

	_state.window = NULL;
}

bool engine_init(engine_config config) {

	if (_state.init || _state.running) { return false; }

	// Platform & Window:
	if (!platform_init(config.platform_type)) {
		LOG_ERR("couldn't initialize platform");
		return false;
	}

	_state.window =
		platform_create_window(config.window_size, config.window_title);
	if (!_state.window) {
		LOG_ERR("couldn't create window");
		return false;
	}
	_state.config = config;

	// Timing:
	{
		_state.timer.game_timestep_s = config.game_timestep_s;
		// _state.timer.render_target_frametime_s = config.target_frametime_s;
		_state.timer.current_time_s = platform_get_time_s();
		_state.timer.previous_time_s = _state.timer.current_time_s;
	}

	// Input:
	if (!engine_input_init()) { LOG_ERR("couldn't initialize input"); }

	// Arenas:
	if (!_create_arenas()) {
		LOG_ERR("couldn't create arenas");
		return false;
	}

	// GPU:
	if (!_init_gpu()) {
		LOG_ERR("couldn't initialize gpu");
		return false;
	}

	// Game
	if (!_load_game()) {
		LOG_ERR("couldn't load game dll");
		return false;
	}

	_state.init = true;

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

	engine_timer* t = &_state.timer;
	gpu_api* gpu = &_state.gpu;
	bool _quit = false;

	while (!_quit && !game_funcs.game_should_quit()) {

		{ // Timer update
			double new_time_s = platform_get_time_s();
			t->frame_time_s = new_time_s - t->current_time_s;
			t->previous_time_s = t->current_time_s;
			t->current_time_s = new_time_s;
			t->frame_accumulator_s += MIN(t->frame_time_s, MAX_FRAMETIME_S);
		}

		{ // Poll input
			platform_poll();
			if (platform_window_should_close(_state.window)) { break; }

			engine_input_state const* input_state = engine_input_update();
			game_funcs.game_process_input(input_state);
		}

		// Begin GPU frame
		gpu_command_buffer cmd = {0};
		gpu_frame frame = {0};
		{
			gpu_frame_result frame_res = gpu->begin_frame(gpu);
			if (frame_res.status == FRAME_ERROR) {
				_quit = true;
				break;
			}
			if (frame_res.status == FRAME_SKIP) {
				gpu->end_frame(gpu, &frame_res.frame);
				LOG_INFO("SKIPPING FRAME");
				continue;
			}
			frame = frame_res.frame;
			cmd = frame.frame_cmd;
		}

		// Step game many times at fixed rate
		LOG_WARN("=== ENGINE FRAME ===");
		while (t->frame_accumulator_s >= t->game_timestep_s) {
			if (!game_funcs.game_update(t->game_timestep_s)) {
				LOG_ERR("couldn't update game");
				exit_status = false;
				break;
			}

			t->frame_accumulator_s -= t->game_timestep_s;
		}

		if (exit_status == false) break;

		// Game draws only once per frame
		game_funcs.game_draw();

		{ // GPU Draw frame:
			gpu_color_target color_target = {
				.clear_color = (gpu_color){.floats = {.1f, .1f, .1f, 1.f}},
				.load_op = GPU_LOAD_OP_CLEAR,
				.store_op = GPU_STORE_OP_STORE,
				.target = frame.framebuffer_view,
			};
			gpu_render_state s = {
				.color_target_count = 1,
				.color_targets = {color_target},
			};

			gpu->cmd_reset_viewport(gpu, cmd);
			gpu->cmd_reset_scissor(gpu, cmd);
			gpu->cmd_begin_rendering(gpu, cmd, &s);
			gpu->cmd_end_rendering(gpu, cmd);
			ASSERT(gpu->end_frame(gpu, &frame));
		}

		/**
		 * [NOTE]: no sleep because vsync is on. don't know if I should rely on
		 * sleep or vsync yet.
		 */

		t->current_frame++;
	}

	_state.running = false;

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
		_state.game_dll_handle = NULL;
		_state.game_loaded = false;
	}
	_state.running = false;

	_state.gpu.wait_idle(&_state.gpu);
	_state.gpu.destroy(&_state.gpu);

	if (!_destroy_arenas()) {
		LOG_ERR("couldn't destroy arenas");
		return false;
	}

	engine_input_shutdown();

	_destroy_platform();
	_state.init = false;

	return true;
}
