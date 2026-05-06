#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <allocators/arena.h>
#include <common.h>
#include <containers/pool.h>
#include <strings/path.h>
#include <visibility.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct window_t window_t;

typedef struct image_data {
	uint32_t width;
	uint32_t height;
	u8_array bytes;
} image_data;

typedef enum gpu_image_format {
	IMAGE_FORMAT_RGBA_UNORM,
	IMAGE_FORMAT_RGBA_SNORM,
	IMAGE_FORMAT_RGBA_SRGB,
	IMAGE_FORMAT_RGB_UNORM,
	IMAGE_FORMAT_RGB_SNORM,
	IMAGE_FORMAT_RGB_SRGB,
	// TODO: add 16 & 32 bit HDR and depth formats
	IMAGE_FORMAT_D16,
} gpu_image_format;

typedef struct {
	uint32_t width;
	uint32_t height;
} gpu_image_extent;

typedef struct {
	gpu_image_extent extent;
	struct {
		uint32_t x;
		uint32_t y;
	} offset;
} gpu_scissor;

typedef union {
	struct {
		float r;
		float g;
		float b;
		float a;
	};
	float floats[4];
} gpu_color;

/**
 * @typedef gpu_image_state
 * @brief Specifies how an image is currently being used. Used for
 * image transitions.
 *
 */
typedef enum gpu_image_state {
	GPU_IMAGE_STATE_NONE = 0,
	GPU_IMAGE_STATE_GENERAL, // temporary?? Vulkan has VK_IMAGE_LAYOUT_GENERAL

	GPU_IMAGE_STATE_SHADER_READ,
	GPU_IMAGE_STATE_SHADER_WRITE,

	GPU_IMAGE_STATE_RENDER_TARGET,

	GPU_IMAGE_STATE_DEPTH_READ,
	GPU_IMAGE_STATE_DEPTH_WRITE,

	GPU_IMAGE_STATE_TRANSFER_SRC,
	GPU_IMAGE_STATE_TRANSFER_DST,
} gpu_image_state;

// clang-format off
typedef uint32_t gpu_image_usage;
#define GPU_IMAGE_USAGE_RENDER_TARGET BIT(1)
#define GPU_IMAGE_USAGE_DEPTH_TARGET  BIT(2)
#define GPU_IMAGE_USAGE_SAMPLED       BIT(3)
// clang-format on

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

typedef struct gpu_shader_ci {
	gpu_shader_type type;
	u8_array code;
	string debug_name;
} gpu_shader_ci;

// clang-format off
typedef struct { pool_handle handle; } gpu_image;
typedef struct { pool_handle handle; } gpu_sampler;
typedef struct { pool_handle handle; } gpu_buffer;
typedef struct { pool_handle handle; } gpu_command_buffer;
typedef struct { pool_handle handle; } gpu_shader;
typedef struct { pool_handle handle; } gpu_pipeline;
typedef struct { void* internal; } gpu_image_view;
// clang-format on

typedef struct gpu_frame {
	gpu_image framebuffer;
	gpu_image_view framebuffer_view;
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

typedef struct gpu_color_target {
	// [NOTE]: format is stored into image, no duplication
	gpu_image target;
	gpu_target_load_op load_op;
	gpu_target_store_op store_op;
	gpu_color clear_color;
	// [TODO]: mip levels, MSAA resolve
} gpu_color_target;

typedef struct gpu_color_target_info {
	gpu_image_format format;
	bool blend;
} gpu_color_target_info;

typedef struct gpu_depth_target {
	gpu_image target;
	gpu_target_load_op load_op;
	gpu_target_store_op store_op;
	float clear_value;
} gpu_depth_target;

#define GPU_MAX_RENDERTARGETS 8
typedef struct gpu_render_state {
	uint32_t color_target_count;
	gpu_color_target color_targets[GPU_MAX_RENDERTARGETS];

	// [NOTE]: Depth target may be omitted depending on pipeline setup
	gpu_depth_target* depth_target;
} gpu_render_state;

typedef enum gpu_primitive_type {
	GPU_PRIMITIVE_TYPE_TRIANGLELIST,
	GPU_PRIMITIVE_TYPE_TRIANGLESTRIP,
	GPU_PRIMITIVE_TYPE_TRIANGLEFAN,
	GPU_PRIMITIVE_TYPE_LINELIST,
	GPU_PRIMITIVE_TYPE_LINESTRIP,
	GPU_PRIMITIVE_TYPE_POINTLIST
} gpu_primitive_type;

typedef enum gpu_compare_op {
	GPU_COMPARE_OP_LESS,
	GPU_COMPARE_OP_LESS_EQUAL,
	GPU_COMPARE_OP_GREATER,
	GPU_COMPARE_OP_GREATER_EQUAL,
	GPU_COMPARE_OP_ALWAYS,
	GPU_COMPARE_OP_NEVER,
} gpu_compare_op;

typedef enum gpu_cull_mode {
	GPU_CULL_MODE_NONE,
	GPU_CULL_MODE_FRONT,
	GPU_CULL_MODE_BACK,
} gpu_cull_mode;

typedef enum gpu_polygon_mode {
	GPU_POLYGON_FILL,
	GPU_POLYGON_LINE,
	GPU_POLYGON_POINT,
} gpu_polygon_mode;

typedef enum gpu_winding {
	GPU_WINDING_CLOCKWISE,
	GPU_WINDING_COUNTER_CLOCKWISE,
} gpu_winding;

typedef struct {
	gpu_shader vertex_shader;
	gpu_shader fragment_shader;

	gpu_color_target_info color_targets[GPU_MAX_RENDERTARGETS];
	uint32_t color_target_count;

	struct {
		gpu_primitive_type primitive_type;
		gpu_cull_mode cull_mode;
		gpu_winding winding_order;
		gpu_polygon_mode polygon_mode;
	} rasterizer;

	/**
	 * [NOTE]: if `depth_test` or `depth_write` is enabled, then
	 * `gpu_render_state.depth_target` must be a valid target
	 */
	struct {
		bool depth_test;
		bool depth_write;
		gpu_compare_op depth_compare_op;
		gpu_image_format depth_target_format;
	} depth;

	struct {
		bool stencil_test;
		gpu_compare_op stencil_compare_op;
		// [TODO]: Front & back distinction
		// [TODO]: Stencil target
	} stencil;

	// [TODO]: multisampling
	// [TODO]: maybe vertex (only bindless buffer device address for now)
} gpu_pipeline_ci;

typedef enum gpu_sampler_filter_mode {
	GPU_SAMPLER_FILTER_NEAREST,
	GPU_SAMPLER_FILTER_LINEAR,
} gpu_sampler_filter_mode;

typedef enum gpu_sampler_address_mode {
	GPU_SAMPLER_ADRESS_REPEAT,
	GPU_SAMPLER_ADRESS_REPEAT_MIRROR,
	GPU_SAMPLER_ADRESS_CLAMP,
} gpu_sampler_address_mode;

typedef enum gpu_sampler_mipmap_mode {
	GPU_SAMPLER_MIPMAP_NEAREST,
	GPU_SAMPLER_MIPMAP_LINEAR,
} gpu_sampler_mipmap_mode;

/**
 * [NOTE]: Goal here is to expose only what's necessary for GLTF.
 * This mean hiding away some features:
 * - lod biasing/clamping
 * - anisotropy
 * - compare against reference value at lookup
 * -> might need some of them for other usages?
 */
typedef struct gpu_sampler_ci {
	gpu_sampler_filter_mode min_filter;
	gpu_sampler_filter_mode mag_filter;
	gpu_sampler_mipmap_mode mipmap_mode;
	gpu_sampler_address_mode address_mode_u;
	gpu_sampler_address_mode address_mode_v;
	gpu_sampler_address_mode address_mode_w;
} gpu_sampler_ci;

typedef enum gpu_image_type {
	GPU_IMAGE_TYPE_2D,
	GPU_IMAGE_TYPE_2D_ARRAY,
	GPU_IMAGE_TYPE_CUBE,
	GPU_IMAGE_TYPE_CUBE_ARRAY,
} gpu_image_type;

typedef struct gpu_image_ci {
	gpu_image_type type;
	gpu_image_format format;
	gpu_image_extent dimensions;
	gpu_image_usage usage;
	uint32_t array_layers;
	uint32_t mip_levels;
} gpu_image_ci;

typedef struct gpu_image_view_ci {
	gpu_image image;
	uint32_t base_layer;
	uint32_t layer_count;
	uint32_t base_level;
	uint32_t level_count;
} gpu_image_view_ci;

/**
 * @typedef gpu_texture
 * @brief Texture is a non-owning view into an image, coupled with a sampler.
 * Both are created separately and owned by API, so no resource management on
 * texture side.
 * For now, textures will be created over an image's full view (all mip
 * levels+array layers), I'm just future-proofing here.
 *
 */
typedef struct gpu_texture {
	gpu_image_view image; // Most often image's full view
	gpu_sampler sampler;
} gpu_texture;

/* ========================================================================== */
typedef struct gpu_api_config {
	gpu_api_type type;
	char const* app_name;
	uint32_t frames_in_flight;
	gpu_image_format target_image_format;
	// TODO: depth format
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
	 * @brief Initialize GPU API. Make sure `type`, `name` and `config` are
	 * set before calling. ([TODO]: refactor this it sucks)
	 *
	 * @param arena Allocations for all resources
	 * @param scratch Temporary
	 */
	void (*initialize)(struct gpu_api*, window_t*, arena_t* arena,
					   arena_t* scratch);

	/**
	 * @brief Release and invalidate everything. Don't call while a frame is
	 * in flight.
	 */
	void (*destroy)(struct gpu_api*);

	void (*wait_idle)(struct gpu_api*);

	/*== Frame Flow ==*/
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
	 * @brief Specify a new usage for image. Image has to have been created
	 * with support the new usage.
	 *
	 */
	bool (*cmd_transition_image)(struct gpu_api*, gpu_command_buffer, gpu_image,
								 gpu_image_state new_state);

	// present is a special layout. image must be a frame's backbuffer

	/**
	 * @brief Same as cmd_transition_image, but presenting is a special
	 * layout that doesn't apply to all images. Given image must be a
	 * frame's backbuffer.
	 *
	 * @return false if image or command buffer aren't valid, true
	 * otherwise.
	 */
	bool (*cmd_transition_for_present)(struct gpu_api*, gpu_command_buffer,
									   gpu_image);

	/**
	 * @brief End command buffer recording, and submit it to GPU for
	 * execution.
	 *
	 */
	void (*cmd_submit)(struct gpu_api*, gpu_command_buffer);

	void (*cmd_set_viewport)(struct gpu_api*, gpu_command_buffer,
							 gpu_image_extent extent);
	void (*cmd_set_scissor)(struct gpu_api*, gpu_command_buffer,
							gpu_scissor scissor);

	void (*cmd_reset_viewport)(struct gpu_api*, gpu_command_buffer);
	void (*cmd_reset_scissor)(struct gpu_api*, gpu_command_buffer);

	void (*cmd_bind_pipeline)(struct gpu_api*, gpu_command_buffer,
							  gpu_pipeline);

	void (*cmd_draw)(struct gpu_api*, gpu_command_buffer, uint32_t vertex_count,
					 uint32_t instance_count, uint32_t first_vertex,
					 uint32_t first_instance);

	/**
	 * @brief End the current frame. Must be called before requesting a new
	 * frame.
	 * This will present the frame's backbuffer to the screen.
	 *
	 */
	void (*end_frame)(struct gpu_api*, gpu_frame*);

	/*== Resource Management ==*/
	gpu_shader (*create_shader)(struct gpu_api*, gpu_shader_ci*);
	void (*destroy_shader)(struct gpu_api*, gpu_shader);

	gpu_pipeline (*create_pipeline)(struct gpu_api*, gpu_pipeline_ci*);
	void (*destroy_pipeline)(struct gpu_api*, gpu_pipeline);

	gpu_image (*create_image)(struct gpu_api*, gpu_image_ci*);
	void (*destroy_image)(struct gpu_api*, gpu_image);
	bool (*create_image_view)(struct gpu_api*, gpu_image_view* out,
							  gpu_image_view_ci*);
	void (*destroy_image_view)(struct gpu_api*, gpu_image_view);
	bool (*temp_copy_to_image)(struct gpu_api*, gpu_command_buffer, gpu_image, image_data);
	void (*temp_set_image_desc)(struct gpu_api*, gpu_image_view);

	gpu_sampler (*create_sampler)(struct gpu_api*, gpu_sampler_ci*);
	void (*destroy_sampler)(struct gpu_api*, gpu_sampler);

	// gpu_texture (*create_texture)(struct gpu_api*, gpu_image_view*,
	// 							  gpu_sampler);
	// gpu_texture (*load_texture)(struct gpu_api*, path image_path,
	// 							gpu_sampler /* , TODO */);

	/*== Getters ==*/
	gpu_image_extent (*get_image_extent)(struct gpu_api*, gpu_image);
	gpu_image_format (*get_image_format)(struct gpu_api*, gpu_image);
	bool (*get_image_full_view)(struct gpu_api*, gpu_image_view* out,
								gpu_image);
} gpu_api;

LIB_EXPORT bool gpu_create_api(gpu_api_config config, gpu_api* out);

/* == Pipeline Builder == */
typedef struct gpu_pipeline_builder {
	gpu_pipeline_ci ci;
} gpu_pipeline_builder;

LIB_EXPORT void gpu_pipeline_builder_create(gpu_pipeline_builder* out);
LIB_EXPORT gpu_pipeline gpu_pipeline_build(gpu_api* gpu,
										   gpu_pipeline_builder* builder);
LIB_EXPORT void set_vertex_shader(gpu_pipeline_builder* builder, gpu_shader vs);
LIB_EXPORT void set_fragment_shader(gpu_pipeline_builder* builder,
									gpu_shader fs);
LIB_EXPORT void add_color_target(gpu_pipeline_builder* builder,
								 gpu_image_format format, bool blend);
// Optional:
LIB_EXPORT void enable_depth_write(gpu_pipeline_builder* builder);
LIB_EXPORT void enable_depth_test(gpu_pipeline_builder* builder);
LIB_EXPORT void set_depth_format(gpu_pipeline_builder* builder,
								 gpu_image_format format);
LIB_EXPORT void set_depth_compare_op(gpu_pipeline_builder* builder,
									 gpu_compare_op op);
LIB_EXPORT void set_primitive_type(gpu_pipeline_builder* builder,
								   gpu_primitive_type type);
LIB_EXPORT void set_cull_mode(gpu_pipeline_builder* builder,
							  gpu_cull_mode mode);
LIB_EXPORT void set_polygon_mode(gpu_pipeline_builder* builder,
								 gpu_polygon_mode mode);
LIB_EXPORT void set_winding_order(gpu_pipeline_builder* builder,
								  gpu_winding winding);
// [TODO]: Stencil, Blending, MSAA

/* ========================================================================== */

#endif // !GRAPHICS_H
