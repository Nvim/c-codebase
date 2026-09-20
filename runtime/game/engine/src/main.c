#include "engine.h"

#include <core/log.h>
#include <string.h>

static platform_type _parse_platform_arg(int argc, char** argv);

static double const GAME_TICKRATE_S = 60;
static double const GAME_TIMESTEP_S = 1.f / (float)GAME_TICKRATE_S;

int main(int argc, char** argv) {

	int exit_status = 0;

	platform_type pf_type = _parse_platform_arg(argc, argv);
	char title[256] = "Game - ";
	char* window_title = strcat(title, platform_type_name(pf_type));

	engine_config config = {0};
	{
		config.platform_type = pf_type;
		config.window_size = (window_size){1280, 720};
		config.window_title = window_title;
		config.game_width = 320.f;
		config.game_height = 180.f;
		config.max_engine_memory_bytes = 2 * MEMORY_GIB;
		config.max_game_memory_bytes = 2 * MEMORY_GIB;
		config.max_frame_memory_bytes = 0; // TODO
		config.game_timestep_s = GAME_TIMESTEP_S;
	}

	if (!engine_init(config)) {
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
