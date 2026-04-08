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

/* ===========================================================================
 * Resource pools
 * ========================================================================== */
// #define VK_IMAGE_POOL_SIZE 1024
// #define VK_BUFFER_POOL_SIZE 1024
// #define VK_COMMAND_BUFFER_POOL_SIZE 1024
//
// // 32-bit handle layout:
// // [ generation (12 bits) | index (20 bits) ]
// #define GPU_INDEX_BITS 20
// #define GPU_GENERATION_BITS 12
// #define GPU_INDEX_MASK ((1u << GPU_INDEX_BITS) - 1)
// #define GPU_GENERATION_MASK ((1u << GPU_GENERATION_BITS) - 1)
//
// #define GPU_INVALID_HANDLE 0
//
// static inline uint32_t gpu_handle_create(uint32_t index, uint32_t generation)
// { 	return ((generation & GPU_GENERATION_MASK) << GPU_INDEX_BITS) |
// (index & GPU_INDEX_MASK);
// }
//
// static inline uint32_t gpu_handle_index(uint32_t handle) {
// 	return handle & GPU_INDEX_MASK;
// }
//
// static inline uint32_t gpu_handle_generation(uint32_t handle) {
// 	return (handle >> GPU_INDEX_BITS) & GPU_GENERATION_MASK;
// }
//
typedef enum {
	IMAGE_FLAGS_SWAPCHAIN = BIT(1),
} vk_image_flags;

typedef struct vk_image_data {
	VkImage image;
	VkImageView image_view;
	VmaAllocation alloc;
	VkExtent2D extent;
	VkFormat format;
	VkImageUsageFlags usage_flags;
	gpu_resource_usage gpu_usage;
	uint8_t vk_image_flags;
} vk_image_data;

STATIC_ASSERT(sizeof(vk_image_data) == 48, "pretty tight");
POOL_DEFINE_TYPE(vk_image_data, vk_image_pool)
// TODO: Make this less global
extern vk_image_pool _image_pool;

vk_image_pool_handle vk_image_create(VkImage image, VkImageView image_view,
									 VmaAllocation alloc, VkExtent2D extent,
									 VkFormat format, VkImageUsageFlags flags,
									 gpu_resource_usage resource_usage,
									 vk_image_flags custom_flags);
vk_image_data* vk_image_get(vk_image_pool_handle handle);

typedef struct vk_cmdbuf_data {
	VkCommandBuffer command_buffer;
} vk_cmdbuf_data;

POOL_DEFINE_TYPE(vk_cmdbuf_data, vk_cmdbuf_pool)
// TODO: Make this less global
extern vk_cmdbuf_pool _cmd_pool;

vk_cmdbuf_pool_handle vk_command_buffer_create(VkCommandBuffer command_buffer);
vk_cmdbuf_data* vk_command_buffer_get(vk_cmdbuf_pool_handle handle);

/* ============================================================================
 * Types
 * ========================================================================== */

// typedef struct vk_rendertarget {
// 	VkImage image;
// 	VkImageView image_view;
// 	VkExtent2D extent;
// 	VkFormat format;
// } vk_rendertarget;

typedef struct vk_surface {
	VkSurfaceKHR surface;
	VkSurfaceCapabilitiesKHR capabilities;
} vk_surface;

bool vk_surface_create(vk_surface* out, VkPhysicalDevice device,
					   VkSurfaceKHR surface);
bool vk_surface_get_capabilities(vk_surface* surface, VkPhysicalDevice device);
void vk_surface_destroy(vk_surface* surface, VkInstance instance);

typedef struct vk_swapchain {
	VkSwapchainKHR swapchain;
	vk_image_array images;
	vk_image_view_array image_views;
	vk_sem_array present_semaphores; // signaled when ready to be presented
	uint32_t img_count;
	uint32_t img_index; // [NOTE]: This is reset every frame
	VkFormat img_format;
	VkSwapchainCreateInfoKHR ci;
} vk_swapchain;

typedef enum {
	ACQUIRE_ERROR,
	ACQUIRE_OUT_OF_DATE,
	ACQUIRE_SUCCESS,
} vk_swapchain_status;

typedef struct {
	vk_swapchain_status status;
	gpu_image image; // Only valid if status is `ACQUIRE_SUCCESS`.
} vk_sc_image_acquire_result;

bool vk_swapchain_create(vk_swapchain* out, arena_t* arena, VkDevice device,
						 vk_surface* surface, uint32_t width, uint32_t height,
						 uint32_t min_image_count, VkFormat image_format);
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
bool vk_swapchain_present(vk_swapchain* swapchain, VkQueue queue);
void vk_swapchain_destroy(vk_swapchain* swapchain, VkDevice device);

// Used to shove every frame-buffered resource
typedef struct vk_render_sync {
	// Signaled when Swapchain image has been acquired.
	// Wait on this before drawing
	VkSemaphore draw_sem;

	// Signaled with present Semaphore, after frame is done.
	// Waited on before next render.
	VkFence drawn_fence;

	// [NOTE]: Should this have the right to keep the raw handle?
	vk_cmdbuf_pool_handle command_buffer;
	// VkCommandBuffer command_buffer;
} vk_render_sync;

typedef struct vk_sync_buffer {
	vk_render_sync frames[VK_MAX_BUFFERING];
	uint32_t frames_in_flight;
} vk_sync_buffer;

bool vk_render_sync_create(VkDevice device, vk_render_sync* out,
						   VkCommandPool command_pool);
void vk_render_sync_destroy(VkDevice device, vk_render_sync* sync);

bool vk_sync_buffer_create(VkDevice device, vk_sync_buffer* out,
						   uint32_t frames_in_flight,
						   VkCommandPool command_pool);
void vk_sync_buffer_destroy(VkDevice device, vk_sync_buffer* out);

typedef struct {
	uint64_t frame_idx;
	gpu_image rendertarget;
	vk_render_sync* frame_sync;
	VkSemaphore swapchain_semaphore;
} vk_frame;

typedef struct vk_api_state {
	vulkan_init_info pf_init_info;
	VkInstance instance;

	VkPhysicalDevice physical_device;
	uint32_t queue_family_idx;
	VkPhysicalDeviceProperties2 device_props;

	VkDevice device;
	VkQueue queue;
	VmaAllocator allocator;
	VkCommandPool command_pool; // TODO: one pool per frame?

	vk_surface surface;

	VkFormat rendertarget_format;
	vk_swapchain swapchain;

	uint64_t frame_idx;
	vk_sync_buffer sync_buffer;
	bool frame_in_progress;
	uint32_t frames_in_flight;
} vk_api_state;

vk_render_sync* vk_get_current_sync(vk_api_state* state);
bool vk_begin_frame(vk_api_state* state, vk_frame* out,
					window_size window_size);

/* ===========================================================================
 * Utils & conversion
 * ========================================================================== */

// [TODO]: Could be nicer idk
static inline bool check_result(VkResult result) {
	bool ret = result == VK_SUCCESS;
	ASSERT(ret);
	return ret;
}

VkImageSubresourceRange vk_base_subresource_range(void);

VkFormat gpu_to_vk_image_format(gpu_image_format f);
gpu_image_format vk_to_gpu_image_format(VkFormat f);

gpu_image_extent vk_to_gpu_image_extent(VkExtent2D e);

// gpu_rendertarget vk_to_gpu_rendertarget(gpu_image* t);

gpu_frame vk_to_gpu_frame(vk_frame* f);

#endif // !VULKAN_UTIL_H
