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

// Specify how image is used
typedef enum {
	GPU_USAGE_NONE = 0,
	GPU_USAGE_GENERAL, // temporary?? Vulkan has VK_IMAGE_LAYOUT_GENERAL

	GPU_USAGE_SHADER_READ,
	GPU_USAGE_SHADER_WRITE,

	GPU_USAGE_RENDER_TARGET,

	GPU_USAGE_DEPTH_READ,
	GPU_USAGE_DEPTH_WRITE,

	GPU_USAGE_TRANSFER_SRC,
	GPU_USAGE_TRANSFER_DST,
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

/**
 * @typedef gpu_api
 * @brief Backend Graphics API interface.
 * Low-level but provides built-in nice abstraction over swapchain and
 * presenting, with Double/Triple buffering setups.
 * Memory barriers and sync is also made easier to deal with, but isn't
 * fully automated.
 *
 */
typedef struct gpu_api {
	gpu_api_type type;
	bool is_init;
	char const* name;
	gpu_api_config config;
	window_t* window;
	gpu_api_platform_init_info* pf_info; // Allocated on arena by `initialize`
	void* state;						 // Allocated on arena by `initialize`

	/**
	 * @brief Initialize GPU API. Make sure `type`, `name` and `config` are set
	 * before calling. ([TODO]: refactor this it sucks)
	 *
	 * @param arena Allocations for all resources
	 * @param scratch Temporary
	 */
	void (*initialize)(struct gpu_api*, window_t*, arena_t* arena,
					   arena_t* scratch);

	/**
	 * @brief Release and invalidate everything. Don't call while a frame is in
	 * flight.
	 */
	void (*destroy)(struct gpu_api*);

	/**
	 * @brief Begin a new frame. Previous frame must have been ended.
	 *
	 * @return If result.ok is true, new frame.
	 */
	gpu_frame_result (*begin_frame)(struct gpu_api*);

	/**
	 * @brief Begin rendering. Call this before any rendering commands.
	 */
	void (*cmd_begin_rendering)(struct gpu_api*, gpu_command_buffer,
								gpu_render_state*);

	/**
	 * @brief End rendering. It must have been begun beforehand.
	 */
	void (*cmd_end_rendering)(struct gpu_api*, gpu_command_buffer);

	/**
	 * @brief Specify a new usage for image. Image has to have been created with
	 * support the new usage.
	 *
	 */
	bool (*cmd_transition_image)(struct gpu_api*, gpu_command_buffer, gpu_image,
								 gpu_resource_usage new_usage);

	// present is a special layout. image must be a frame's backbuffer

	/**
	 * @brief Same as cmd_transition_image, but presenting is a special layout
	 * that doesn't apply to all images. Given image must be a frame's
	 * backbuffer.
	 *
	 * @return false if image or command buffer aren't valid, true otherwise.
	 */
	bool (*cmd_transition_for_present)(struct gpu_api*, gpu_command_buffer,
									   gpu_image);

	/**
	 * @brief End command buffer recording, and submit it to GPU for execution.
	 *
	 */
	void (*cmd_submit)(struct gpu_api*, gpu_command_buffer);

	/**
	 * @brief End the current frame. Must be called before requesting a new
	 * frame.
	 * This will present the frame's backbuffer to the screen.
	 *
	 */
	void (*end_frame)(struct gpu_api*, gpu_frame*);
} gpu_api;

LIB_EXPORT bool gpu_create_api(gpu_api_config config, gpu_api* out);
/* ========================================================================== */

#endif // !GRAPHICS_H
