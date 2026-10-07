#include "engine.h"

#include <allocators/arena.h>
#include <core/log.h>
#include <loaders/json_loader.h>
#include <string.h>

static platform_type _parse_platform_arg(int argc, char** argv);
static bool _load_config(engine_config* out, arena_t* arena);

// static double const GAME_TICKRATE_S = 60;
// static double const GAME_TIMESTEP_S = 1.f / (float)GAME_TICKRATE_S;

int main(int argc, char** argv) {

	int exit_status = 0;
	platform_type pf_type = _parse_platform_arg(argc, argv);

	if (!arena_pool_init()) {
		LOG_FATAL("couldn't initialize arena pool");
		return 1;
	}

	engine_config conf;
	scratch_arena scratch;
	ASSERT(arena_get_scratch(NULL, &scratch));
	if (!_load_config(&conf, scratch.arena)) {
		LOG_FATAL("couldn't parse config file");
		return 1;
	}

	LOG_INFO("loaded config from file: TODO DUMP");
	conf.platform_type = pf_type;

	if (!engine_init(conf)) {
		LOG_FATAL("couldn't initialize engine");
		return 1;
	}

	if (!engine_run()) {
		LOG_ERR("engine main loop exited with error");
		exit_status = 2;
	}

	if (!engine_shutdown()) {
		LOG_FATAL("couldn't shutdown engine");
		return 1;
	}

	arena_clear_scratch(scratch);
	return exit_status;
}

static platform_type _parse_platform_arg(int argc, char** argv) {
	platform_type type = PLATFORM_TYPE_UNKOWN;
#ifdef PLATFORM_LINUX_WAYLAND
	type = PLATFORM_TYPE_LINUX_WAYLAND;
#else
	type = PLATFORM_TYPE_LINUX_X11;
#endif // PLATFORM_LINUX_WAYLAND

	if (argc == 2) {
		char* arg = argv[1];
		if (strncmp(arg, "--wayland", 9) == 0) {
			type = PLATFORM_TYPE_LINUX_WAYLAND;
		} else if (strncmp(arg, "--x11", 5) == 0) {
			type = PLATFORM_TYPE_LINUX_X11;
		} else {
			LOG_WARN("unknown platform flag. falling back to default");
		}
	}

	LOG_DEBUG("initializing engine for platform %s", platform_type_name(type));
	return type;
}

static bool _load_config(engine_config* out, arena_t* scratch) {
	scratch_arena scratch_2;
	ASSERT(arena_get_scratch(scratch, &scratch_2));

	json_node* cfg_root =
		load_json(scratch, scratch_2.arena, str("./engine_config.json"));
	if (!cfg_root) {
		LOG_ERR("couldn't load config file");
		return false;
	}
	if (!engine_config_parse(out, cfg_root)) {
		LOG_ERR("couldn't parse config file");
		return false;
	}
	arena_clear_scratch(scratch_2);

	return true;
}
