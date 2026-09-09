#ifndef ENGINE_H
#define ENGINE_H

#include <platform/platform.h>

#include <stdbool.h>

typedef struct engine_config {
	platform_type platform_type;
	window_size window_size;
	char const* window_title;

	uint64_t max_engine_memory_bytes;
	uint64_t max_game_memory_bytes;
	uint64_t max_frame_memory_bytes;
} engine_config;

bool engine_init(engine_config config);

bool engine_shutdown(void);

bool engine_run(void);

#endif // !ENGINE_H
