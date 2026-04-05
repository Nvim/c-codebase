#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <allocators/arena.h>
#include <common.h>
#include <visibility.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct window_t window_t;

typedef enum gpu_image_format {
	IMAGE_FORMAT_RGBA_UNORM,
	IMAGE_FORMAT_RGBA_SNORM,
	IMAGE_FORMAT_RGBA_SRGB,
	IMAGE_FORMAT_RGB_UNORM,
	IMAGE_FORMAT_RGB_SNORM,
	IMAGE_FORMAT_RGB_SRGB,
	// TODO: add 16 & 32 bit HDR and depth formats
} gpu_image_format;

typedef struct {
	uint32_t width;
	uint32_t height;
} gpu_image_extent;

typedef union {
	struct {
		float r;
		float g;
		float b;
		float a;
	};
	float floats[4];
} gpu_color;

typedef enum {
	GPU_USAGE_SHADER_READ = BIT(0),
	GPU_USAGE_SHADER_WRITE = BIT(1),
	GPU_USAGE_RENDER_TARGET = BIT(2),
	GPU_USAGE_DEPTH = BIT(3),
	GPU_USAGE_TRANSFER_SRC = BIT(4),
	GPU_USAGE_TRANSFER_DST = BIT(5),
	GPU_USAGE_PRESENT = BIT(6),
} gpu_resource_usage;

typedef enum {
	GPU_BUFFER_USAGE_INDEX,
	GPU_BUFFER_USAGE_STORAGE,
	GPU_BUFFER_USAGE_UNIFORM,
	GPU_BUFFER_USAGE_INDIRECT,
} gpu_buffer_usage;

typedef enum {
	GPU_SHADER_VERTEX,
	GPU_SHADER_FRAGMENT,
	GPU_SHADER_COMPUTE,
} gpu_shader_type;


// clang-format off
typedef struct { uint32_t handle; } gpu_image;
typedef struct { uint32_t handle; } gpu_buffer;
typedef struct { uint32_t handle; } gpu_command_buffer;
// clang-format on

// typedef struct gpu_rendertarget {
// 	gpu_image image;
// 	gpu_image_extent extent;
// 	gpu_image_format format;
// } gpu_rendertarget;

typedef struct gpu_frame {
	// gpu_rendertarget backbuffer;
	gpu_image backbuffer;
	gpu_command_buffer frame_cmd; // Only valid for this frame
	uint64_t frame_index;
} gpu_frame;

typedef enum {
	GPU_LOAD_OP_CLEAR,
	GPU_LOAD_OP_LOAD,
} gpu_target_load_op;

typedef enum {
	GPU_STORE_OP_STORE,
	GPU_STORE_OP_DONT_CARE,
} gpu_target_store_op;

typedef enum {
	GPU_ATTACHMENT_COLOR,
	GPU_ATTACHMENT_DEPTH,
} gpu_attachment_type;

typedef struct gpu_attachment {
	gpu_attachment_type type;
	// gpu_rendertarget* target;
	gpu_image target;
	gpu_target_load_op load_op;
	gpu_target_store_op store_op;
} gpu_attachment;

#define GPU_MAX_RENDERTARGETS 8
typedef struct gpu_rendering_state {
	uint32_t color_target_count;
	// gpu_rendertarget color_targets[GPU_MAX_RENDERTARGETS];
	gpu_image color_targets[GPU_MAX_RENDERTARGETS];
	gpu_color clear_color;
} gpu_render_state;

/* ========================================================================== */
typedef struct gpu_api_config {
	gpu_api_type type;
	char const* app_name;
	uint32_t frames_in_flight;
	gpu_image_format target_image_format;
	// TODO: depth format
	// TODO: alpha blending
	// TODO: vsync mode
} gpu_api_config;

typedef struct gpu_frame_result {
	gpu_frame frame;
	bool ok;
} gpu_frame_result;

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
	gpu_api_config config;
	window_t* window;
	gpu_api_platform_init_info* pf_info; // Allocated on arena by `initialize`
	void* state;						 // Allocated on arena by `initialize`

	void (*initialize)(struct gpu_api*, window_t*, arena_t* arena,
					   arena_t* scratch);
	void (*render)(struct gpu_api*);
	void (*destroy)(struct gpu_api*);
	gpu_frame_result (*begin_frame)(struct gpu_api*);
	void (*cmd_begin_rendering)(struct gpu_api*, gpu_command_buffer cmd,
								gpu_render_state* s);
	void (*cmd_submit)(struct gpu_api*, gpu_command_buffer cmd);
	void (*end_frame)(struct gpu_api*);
} gpu_api;

LIB_EXPORT bool gpu_create_api(gpu_api_config config, gpu_api* out);
/* ========================================================================== */

#endif // !GRAPHICS_H
