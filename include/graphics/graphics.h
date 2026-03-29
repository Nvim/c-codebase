#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <allocators/arena.h>
#include <common.h>
#include <visibility.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct window_t window_t;

typedef enum gpu_image_format {
	IMAGE_FORMAT_RGBA,
	IMAGE_FORMAT_RGB,
} gpu_image_format;

typedef struct gpu_api_config {
	gpu_api_type type;
	char const* app_name;
	uint32_t frames_in_flight;
	gpu_image_format target_image_format;
} gpu_api_config;

/**
 * @typedef graphics_api_init_info
 * @brief Platform provided create info for a graphics API.
 *
 */
typedef struct gpu_api_platform_init_info gpu_api_platform_init_info;

typedef struct gpu_api {
	gpu_api_type type;
	bool is_init;
	char const* name;
	gpu_api_platform_init_info* pf_info;
	void* state; // Allocated on arena by `initialize`

	void (*initialize)(struct gpu_api* self, window_t* window, arena_t* arena,
					   arena_t* scratch);
	void (*destroy)(struct gpu_api* self);
} gpu_api;

LIB_EXPORT bool gpu_create_api(gpu_api_config config, gpu_api* out);

#endif // !GRAPHICS_H
