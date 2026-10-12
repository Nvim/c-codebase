#ifndef ENGINE_H
#define ENGINE_H

#include <platform/platform.h>
#include <strings/path.h>

#include <stdbool.h>

typedef struct engine_config {
	// platform & windw
	platform_type platform_type;
	window_size window_size;
	string window_title;

	/**
	 * [TODO]: Engine needs these for now because it "owns" and initializes
	 * camera. In the future, camera projection should be configured by game, so
	 * logical resolution doesn't leak to engine.
	 * Replace these with `target_aspect_ratio` instead, which is needed for
	 * viewport.
	 */
	double game_width;
	double game_height;

	// memory
	uint64_t max_engine_memory_bytes;
	uint64_t max_game_memory_bytes;
	uint64_t max_frame_memory_bytes;

	// timing
	double game_timestep_s;
	double target_frametime_s;
} engine_config;

bool engine_init(engine_config config);

bool engine_shutdown(void);

bool engine_run(void);

typedef struct json_node json_node;
bool engine_config_parse(engine_config* out, json_node* root);

bool engine_config_reload(path config_file);

#endif // !ENGINE_H
