#ifndef VULKAN_UTIL_H
#define VULKAN_UTIL_H

#include "vulkan_init_info.h"

#include <containers/array.h>
#include <containers/pool.h>
#include <core/core.h>
#include <graphics/graphics.h>
#include <platform/platform.h>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

ARRAY_DEFINE_TYPE(VkImage, vk_image_array)
ARRAY_DEFINE_TYPE(VkImageView, vk_image_view_array)
ARRAY_DEFINE_TYPE(VkSemaphore, vk_sem_array)

#define VK_MAX_BUFFERING 4

#define TEXTURE_BINDING 0
#define STORAGE_IMAGE_BINDING 1
#define SAMPLER_BINDING 2

/* ===========================================================================
 * Resource pools
 * ========================================================================== */

/* == Image == */
typedef enum {
	IMAGE_FLAGS_SWAPCHAIN = BIT(1),
} vk_image_flags;

typedef struct vk_image_data {
	VkImage image;
	VmaAllocation alloc;
	VkExtent2D extent;
	VkFormat format;
	VkImageUsageFlags usage_flags; // Set at creation (not swapchain)
	gpu_image_type gpu_type;
	gpu_image_state gpu_state;
	uint16_t layer_count;
	uint16_t level_count;
	// TODO: view count for validating destruction
	uint8_t vk_image_flags;
} vk_image_data;

STATIC_ASSERT(sizeof(vk_image_data) == 48, "pretty tight");
POOL_DEFINE_TYPE(vk_image_data, vk_image_pool)
#define VK_IMGPOOL_INVALID (vk_image_pool_handle){.raw_handle = 0}

/* == Image View == */
typedef struct {
	uint16_t base_level;
	uint16_t level_count;
	uint16_t base_layer;
	uint16_t layer_count;
} vk_texture_view_infos;

typedef struct {
	uint16_t level;
	uint16_t layer;
} vk_image_view_infos;

typedef struct vk_image_view_data {
	VkImageView view;
	union {
		vk_texture_view_infos texture_info; // Texture view only
		vk_image_view_infos view_info;		// All other image views
	} data;
	vk_image_pool_handle image;
	gpu_image_view_type type;
	uint32_t descriptor_index; // Only for Texture & Storage view
} vk_image_view_data;
POOL_DEFINE_TYPE(vk_image_view_data, vk_image_view_pool)
#define VK_IMGVIEWPOOL_INVALID (vk_image_view_pool_handle){.raw_handle = 0}

/* == Command Buffer == */
typedef enum vk_cmdbuf_state {
	CMDBUF_NOT_BEGAN, // -> VkBeginCommandBuffer not called yet
	CMDBUF_RECORDING,
	CMDBUF_RENDERING, // -> cmd_end_rendering must be called before cmd_submit
	CMDBUF_ENDED,	  // -> VkEndCommandBuffer called. [TODO]: may need info on
					  // async VkQueueSubmit
	CMDBUF_INVALID,
} vk_cmdbuf_state;

typedef uint32_t vk_cmdbuf_flags;
#define CMDBUF_FLAG_FRAME BIT(1)
#define CMDBUF_FLAG_USER BIT(2)

typedef struct vk_cmdbuf_data {
	VkCommandBuffer command_buffer;
	vk_cmdbuf_state state;
	vk_cmdbuf_flags flags;
} vk_cmdbuf_data;

POOL_DEFINE_TYPE(vk_cmdbuf_data, vk_cmdbuf_pool)
#define VK_CMDPOOL_INVALID (vk_cmdbuf_pool_handle){.raw_handle = 0}

/* == Shader Modules == */
typedef struct vk_shader_data {
	VkShaderModule module;
	VkShaderStageFlags stage;
	string name;
} vk_shader_data;

POOL_DEFINE_TYPE(vk_shader_data, vk_shader_pool)
#define VK_SHADERPOOL_INVALID (vk_shader_pool_handle){.raw_handle = 0}

/* == Pipeline == */
typedef struct vk_pipeline_data {
	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkGraphicsPipelineCreateInfo ci;
} vk_pipeline_data;

POOL_DEFINE_TYPE(vk_pipeline_data, vk_pipeline_pool)
#define VK_PIPELINEPOOL_INVALID (vk_pipeline_pool_handle){.raw_handle = 0}

// [NOTE]: alloc_info.size != buffer's actual size, so might not need
// VmaAllocationInfo, just VkMemoryPropertyFlags+Device Address should be enough
typedef struct vk_buffer_data {
	VmaAllocationInfo alloc_info;
	VmaAllocation alloc;
	VkBuffer buffer;
	VkDeviceAddress device_address;
	VkDeviceSize size;
	VkBufferUsageFlagBits2 usage_flags;
	VkMemoryPropertyFlags memory_flags;
	gpu_buffer_access last_access; // [NOTE]: CPU/GPU sync maybe sus
} vk_buffer_data;

POOL_DEFINE_TYPE(vk_buffer_data, vk_buffer_pool)
#define VK_BUFFERPOOL_INVALID(vk_buffer_pool_handle) {.raw_handle = 0}

/**
 * @typedef vk_opaque_handle
 * @brief Type-erased vulkan handle, use for storing Vulkan objects of different
 * types in a common pool.
 *
 */
typedef struct vk_opaque_handle {
	VkObjectType type;
	void* handle;
} vk_opaque_handle;

POOL_DEFINE_TYPE(vk_opaque_handle, vk_handle_pool)
#define VK_HANDLEPOOL_INVALID(vk_handle_pool) {.raw_handle = 0}

/* ============================================================================
 * Types
 * ========================================================================== */

typedef struct {
	VkDeviceAddress reserved_address;
	VkDeviceAddress user_data_buffer;
} vk_push_constants;

typedef struct vk_surface {
	VkSurfaceKHR surface;
	VkSurfaceCapabilitiesKHR capabilities;
} vk_surface;

bool vk_surface_create(vk_surface* out, VkPhysicalDevice device,
					   VkSurfaceKHR surface);
bool vk_surface_get_capabilities(vk_surface* surface, VkPhysicalDevice device);
void vk_surface_destroy(vk_surface* surface, VkInstance instance);

typedef struct vk_swapchain {
	bool must_refresh; // recreate at beginning of next frame

	VkSwapchainKHR swapchain;
	vk_image_array images;
	vk_image_view_array image_views;
	vk_sem_array present_semaphores; // signaled when ready to be presented
	uint32_t img_count;
	uint32_t img_index; // [NOTE]: This is reset every frame
	VkFormat img_format;
	VkSwapchainCreateInfoKHR ci;

	vk_image_pool image_pool;
	vk_image_view_pool image_view_pool;
	pool_handle_array images_pool_handles;
	pool_handle_array image_views_pool_handles;
} vk_swapchain;

typedef enum {
	ACQUIRE_ERROR,
	ACQUIRE_OUT_OF_DATE,
	ACQUIRE_SUCCESS,
} vk_swapchain_status;

typedef struct {
	vk_swapchain_status status;
	gpu_image image;	 // Only valid if status is `ACQUIRE_SUCCESS`.
	gpu_image_view view; // Only valid if status is `ACQUIRE_SUCCESS`.
} vk_sc_image_acquire_result;

typedef enum {
	PRESENT_ERROR,
	PRESENT_OUT_OF_DATE,
	PRESENT_SUCCESS,
} vk_present_result;

bool vk_swapchain_create(vk_swapchain* out, arena_t* arena, VkDevice device,
						 vk_surface* surface, uint32_t width, uint32_t height,
						 uint32_t min_image_count, VkFormat image_format,
						 vk_image_pool image_pool,
						 vk_image_view_pool view_pool);
bool vk_swapchain_refresh(vk_swapchain* swapchain, VkDevice device,
						  vk_surface* surface, uint32_t width, uint32_t height);
bool vk_swapchain_get_current_image(vk_swapchain* swapchain, VkImage* out);
bool vk_swapchain_get_current_image_view(vk_swapchain* swapchain,
										 VkImageView* out);
bool vk_swapchain_get_current_semaphore(vk_swapchain* swapchain,
										VkSemaphore* out);
vk_sc_image_acquire_result vk_swapchain_acquire_image(vk_swapchain* swapchain,
													  VkDevice device,
													  VkSemaphore semaphore);
vk_present_result vk_swapchain_present(vk_swapchain* swapchain, VkQueue queue);
void vk_swapchain_destroy(vk_swapchain* swapchain, VkDevice device);

// Used to shove every frame-buffered resource
typedef struct vk_render_sync {
	// Signaled when Swapchain image has been acquired.
	// Wait on this before drawing
	VkSemaphore draw_sem;

	// Signaled with present Semaphore, after frame is done.
	// Waited on before next render.
	VkFence drawn_fence;

	vk_cmdbuf_pool_handle command_buffer;
} vk_render_sync;

typedef struct vk_sync_buffer {
	vk_render_sync frames[VK_MAX_BUFFERING];
	uint32_t frames_in_flight;
} vk_sync_buffer;

bool vk_render_sync_create(VkDevice device, vk_render_sync* out,
						   vk_cmdbuf_pool cmd_pool, VkCommandPool command_pool);
void vk_render_sync_destroy(VkDevice device, vk_render_sync* sync);

bool vk_sync_buffer_create(VkDevice device, vk_sync_buffer* out,
						   vk_cmdbuf_pool cmd_pool, uint32_t frames_in_flight,
						   VkCommandPool command_pool);
void vk_sync_buffer_destroy(VkDevice device, vk_sync_buffer* out);

typedef struct {
	bool has_rendertarget; // False if swapchain acquire failed, frame should be
						   // skipped
	uint64_t frame_idx;
	vk_image_pool_handle rendertarget;
	vk_image_view_pool_handle rendertarget_view;
	vk_render_sync* frame_sync;
	VkSemaphore swapchain_semaphore;
} vk_frame;

typedef struct vk_api_state {
	arena_t* arena; // All heap allocations are made here
	// Initialization stuff:
	vulkan_init_info pf_init_info;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	uint32_t queue_family_idx;
	VkPhysicalDeviceFeatures2 device_features;
	VkPhysicalDeviceProperties2 device_props;
	VkPhysicalDeviceMemoryProperties2 memory_props;
	VkPhysicalDeviceDescriptorIndexingProperties descriptor_props;
	VkPhysicalDeviceVulkan14Properties vulkan_props;
	uint64_t max_buffer_size;
	uint64_t max_buffer_range;
	uint64_t push_constant_size;

	// Base handles
	VkDevice device;
	VkQueue queue;
	VmaAllocator allocator;
	VkCommandPool command_pool; // [TODO]: one pool per frame?

	VkDescriptorPool descriptor_pool;
	VkDescriptorSetLayout descriptor_layout;
	VkDescriptorSet descriptor;

	// Swapchain
	vk_surface surface;
	VkFormat rendertarget_format;
	vk_swapchain swapchain;

	// Frame-in-flight buffering
	uint64_t frame_idx;
	vk_sync_buffer sync_buffer;
	bool frame_in_progress;
	uint32_t frames_in_flight;

	// Resource pools:
	vk_image_pool image_pool;
	vk_image_view_pool image_view_pool;
	vk_cmdbuf_pool cmd_pool;
	vk_shader_pool shader_pool;
	vk_pipeline_pool pipeline_pool;
	vk_buffer_pool buffer_pool;
	vk_handle_pool handle_pool;
} vk_api_state;

vk_render_sync* vk_get_current_sync(vk_api_state* state);
bool vk_begin_frame(vk_api_state* state, vk_frame* out,
					window_size window_size);

/* ===========================================================================
 * Image Layouts, Capabilites and Barriers
 * ========================================================================== */
static VkMemoryBarrier2 const vk_full_pipeline_barrier = {
	.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
	.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
	.srcAccessMask =
		VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
	.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
	.dstAccessMask =
		VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
};

static VkImageUsageFlags gpu_image_state_to_vk_state_flags[] = {
	[GPU_IMAGE_STATE_NONE] = 0,

	// [TODO]: Not sure about how to handle GENERAL:
	[GPU_IMAGE_STATE_GENERAL] =
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	[GPU_IMAGE_STATE_SHADER_READ] = VK_IMAGE_USAGE_SAMPLED_BIT,
	[GPU_IMAGE_STATE_SHADER_WRITE] = VK_IMAGE_USAGE_STORAGE_BIT,
	[GPU_IMAGE_STATE_RENDER_TARGET] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	[GPU_IMAGE_STATE_DEPTH_READ] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	[GPU_IMAGE_STATE_DEPTH_WRITE] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	[GPU_IMAGE_STATE_TRANSFER_SRC] = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	[GPU_IMAGE_STATE_TRANSFER_DST] = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
};

typedef struct vk_image_barrier_info {
	VkImageLayout layout;
	VkAccessFlags2 access;
	VkPipelineStageFlags2 stage;
} vk_image_barrier_info;

/**
 * @brief Barrier lookup table. Not sure about some states, will mark them with
 * todos.
 * No shader stage granularity, I use fragment for all.
 *
 * - Sources:
 * https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#graphics-to-graphics-dependencies
 * https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/05_rendering_pipeline.html#_image_layout_transition_barrier_configuration_and_resource_specification
 */
static vk_image_barrier_info gpu_state_to_vk_image_barrier[] = {
	[GPU_IMAGE_STATE_NONE] =
		{
			.layout = VK_IMAGE_LAYOUT_UNDEFINED,
			.access = VK_ACCESS_2_NONE,
			.stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		},
	[GPU_IMAGE_STATE_GENERAL] =
		{
			// [TODO]: won't use it for color attachment, but it technically
			// could?
			.layout = VK_IMAGE_LAYOUT_GENERAL,
			.access =
				VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
			.stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		},
	[GPU_IMAGE_STATE_SHADER_READ] =
		{
			.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.access = VK_ACCESS_2_SHADER_READ_BIT,
			.stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		},
	[GPU_IMAGE_STATE_SHADER_WRITE] =
		{
			.layout = VK_IMAGE_LAYOUT_GENERAL,
			.access =
				VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT,
			.stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		},
	[GPU_IMAGE_STATE_RENDER_TARGET] =
		{
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
					  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		},
	[GPU_IMAGE_STATE_DEPTH_READ] =
		{
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			.access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			.stage = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		},
	[GPU_IMAGE_STATE_DEPTH_WRITE] =
		{
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.stage = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		},
	[GPU_IMAGE_STATE_TRANSFER_SRC] =
		{
			.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.access = VK_ACCESS_2_TRANSFER_READ_BIT,
			.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		},
	[GPU_IMAGE_STATE_TRANSFER_DST] =
		{
			.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		},
};

static inline VkImageLayout
_gpu_image_state_to_vk_image_layout(gpu_image_state s) {
	return gpu_state_to_vk_image_barrier[s].layout;
}

void vk_image_barrier(VkCommandBuffer cmd_buf, VkImage image,
					  vk_image_barrier_info from, vk_image_barrier_info to);

bool vk_transition_to_present(vk_api_state* state, gpu_frame* frame);

bool has_image_descriptor(gpu_image_view_type view_type);

#define INVALID_DESCRIPTOR UINT32_MAX
bool vk_update_image_descriptor(vk_api_state* state,
								vk_image_view_pool_handle view);

uint32_t vk_update_sampler_descriptor(vk_api_state* state,
									  vk_handle_pool_handle handle);

typedef struct vk_buffer_barrier_info {
	VkAccessFlags2 access;
	VkPipelineStageFlags2 stage;
} vk_buffer_barrier_info;

static vk_buffer_barrier_info gpu_buffer_access_to_vk_state_flags[] = {
	[GPU_BUFFER_ACCESS_TRANSFER_WRITE] =
		{
			.access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		},
	[GPU_BUFFER_ACCESS_INDEX_READ] =
		{
			.access = VK_ACCESS_2_INDEX_READ_BIT,
			.stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
		},
	[GPU_BUFFER_ACCESS_INDIRECT_DRAW_READ] =
		{
			.access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
			.stage = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
		},
	[GPU_BUFFER_ACCESS_VERTEX_SHADER_READ] =
		{
			.access = VK_ACCESS_2_SHADER_READ_BIT,
			.stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
		},
	[GPU_BUFFER_ACCESS_VERTEX_SHADER_WRITE] =
		{
			.access = VK_ACCESS_2_SHADER_WRITE_BIT,
			.stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
		},
	[GPU_BUFFER_ACCESS_FRAGMENT_SHADER_READ] =
		{
			.access = VK_ACCESS_2_SHADER_READ_BIT,
			.stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		},
	[GPU_BUFFER_ACCESS_FRAGMENT_SHADER_WRITE] =
		{
			.access = VK_ACCESS_2_SHADER_WRITE_BIT,
			.stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		},
};

void vk_buffer_barrier(VkCommandBuffer cmd_buf, VkBuffer image,
					   vk_buffer_barrier_info from, vk_buffer_barrier_info to);

bool vk_submit_frame_cmd(vk_api_state* state, gpu_frame* frame);

/* ===========================================================================
 * Utils & conversion
 * ========================================================================== */

/**
 * @brief Create a single 2D image. Mip levels and image arrays are supported.
 * The full view spanning all mips and layers will be created, but no pool
 * handle will be created for it.
 *
 */
vk_image_pool_handle vk_create_2d_image(vk_api_state* state, VkExtent2D extent,
										VkFormat format, gpu_image_type type,
										VkImageUsageFlags usage,
										uint32_t levels, uint32_t layers,
										uint32_t queue_family_idx);

/**
 * @brief Allocate a view in the pool for the image's full view.
 *
 */
// vk_image_view_pool_handle
// vk_create_full_image_view_handle(vk_api_state* state, vk_image_pool_handle
// h);

bool gpu_to_vk_pipeline_ci(arena_t* arena, gpu_pipeline_ci* ci,
						   VkGraphicsPipelineCreateInfo* out,
						   vk_api_state* state);

// [TODO]: Could be nicer idk
static inline bool check_result(VkResult result) {
	bool ret = result == VK_SUCCESS;
	ASSERT(ret);
	return ret;
}

static inline vk_image_pool_handle to_vk_img(gpu_image handle) {
	return vk_image_pool_wrap(handle.handle);
}

static inline vk_image_view_pool_handle to_vk_imgview(gpu_image_view handle) {
	return vk_image_view_pool_wrap(handle.handle);
}

static inline vk_cmdbuf_pool_handle to_vk_cmdbuf(gpu_command_buffer handle) {
	return vk_cmdbuf_pool_wrap(handle.handle);
}

static inline vk_shader_pool_handle to_vk_shader(gpu_shader handle) {
	return vk_shader_pool_wrap(handle.handle);
}

static inline vk_pipeline_pool_handle to_vk_pipeline(gpu_pipeline handle) {
	return vk_pipeline_pool_wrap(handle.handle);
}

static inline vk_buffer_pool_handle to_vk_buffer(gpu_buffer handle) {
	return vk_buffer_pool_wrap(handle.handle);
}

#define to_vk_handle(h) vk_handle_pool_wrap(h.handle)

VkImageSubresourceRange vk_full_subresource_range(VkImageUsageFlagBits usage,
												  uint32_t layers,
												  uint32_t levels);

VkImageSubresourceRange vk_custom_subresource_range(VkImageUsageFlagBits usage,
													uint32_t layers,
													uint32_t levels,
													uint32_t base_level,
													uint32_t base_layer);

VkImageSubresourceRange vk_base_subresource_range(void);

VkFormat gpu_to_vk_image_format(gpu_image_format f);
gpu_image_format vk_to_gpu_image_format(VkFormat f);

gpu_image_extent vk_to_gpu_image_extent(VkExtent2D e);
VkExtent2D gpu_to_vk_image_extent(gpu_image_extent e);

// gpu_rendertarget vk_to_gpu_rendertarget(gpu_image* t);

gpu_frame vk_to_gpu_frame(vk_frame* f);

VkShaderStageFlags gpu_to_vk_shader_stage(gpu_shader_type t);

VkPrimitiveTopology gpu_to_vk_primitive_type(gpu_primitive_type t);

VkCompareOp gpu_to_vk_compare_op(gpu_compare_op op);

VkCullModeFlagBits gpu_to_vk_cull_mode(gpu_cull_mode m);

VkPolygonMode gpu_to_vk_polygon_mode(gpu_polygon_mode m);

VkFrontFace gpu_to_vk_winding(gpu_winding w);

VkImageViewType gpu_to_vk_image_type(gpu_image_type t);

VkFilter gpu_to_vk_filter(gpu_sampler_filter_mode f);

VkSamplerMipmapMode gpu_to_vk_mipmap_mode(gpu_sampler_mipmap_mode m);

VkSamplerAddressMode gpu_to_vk_address_mode(gpu_sampler_address_mode);

VkBufferUsageFlagBits2 gpu_to_vk_buffer_usage(gpu_buffer_usage u);

VmaAllocationCreateInfo gpu_to_vk_buffer_memory(gpu_buffer_ci* ci);

VkAttachmentStoreOp gpu_to_vk_store_op(gpu_target_store_op op);

VkAttachmentLoadOp gpu_to_vk_load_op(gpu_target_load_op op);

#endif // !VULKAN_UTIL_H
