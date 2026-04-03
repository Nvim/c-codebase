#ifndef VULKAN_UTIL_H
#define VULKAN_UTIL_H

#include "vulkan_init_info.h"

#include <containers/array.h>
#include <core/core.h>
#include <graphics/graphics.h>
#include <platform/platform.h>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

ARRAY_DEFINE_TYPE(VkImage, vk_image_array)
ARRAY_DEFINE_TYPE(VkImageView, vk_image_view_array)
ARRAY_DEFINE_TYPE(VkSemaphore, vk_sem_array)

#define VK_MAX_BUFFERING 4

typedef struct vk_rendertarget {
	VkImage image;
	VkImageView image_view;
	VkExtent2D extent;
} vk_rendertarget;

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
	ACQUIRE_SUCCESS,
	ACQUIRE_OUT_OF_DATE,
	ACQUIRE_ERROR,
} vk_sc_image_acquire_result;

bool vk_swapchain_create(vk_swapchain* out, arena_t* arena, VkDevice device,
						 vk_surface* surface, uint32_t width, uint32_t height,
						 uint32_t min_image_count, VkFormat image_format);
bool vk_swapchain_refresh(vk_swapchain* swapchain, VkDevice device,
						  vk_surface* surface, uint32_t width, uint32_t height);
vk_sc_image_acquire_result vk_swapchain_acquire_image(vk_swapchain* swapchain,
													  VkDevice device,
													  vk_rendertarget* out,
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

	VkCommandBuffer command_buffer;
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
} vk_api_state;

// [TODO]: Could be nicer idk
static inline bool check_result(VkResult result) {
	bool ret = result == VK_SUCCESS;
	ASSERT(ret);
	return ret;
}

VkFormat to_vk_format(gpu_image_format f);

#endif // !VULKAN_UTIL_H
