#ifndef VULKAN_UTIL_H
#define VULKAN_UTIL_H

#include "vulkan_init_info.h"

#include <containers/array.h>
#include <core/core.h>
#include <platform/platform.h>

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

ARRAY_DEFINE_TYPE(VkImage, vk_image_array)
ARRAY_DEFINE_TYPE(VkImageView, vk_image_view_array)

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
	uint32_t img_count;
	uint32_t img_index;
	VkFormat img_format;
	VkSwapchainCreateInfoKHR ci;
} vk_swapchain;

// bool vk_swapchain_refresh(vk_swapchain* swapchain);
bool vk_swapchain_create(vk_swapchain* out, arena_t* arena, VkDevice device,
						 vk_surface* surface, uint32_t width, uint32_t height,
						 uint32_t min_image_count, VkFormat image_format);
bool vk_swapchain_refresh(vk_swapchain* swapchain, VkDevice device,
						  vk_surface* surface, uint32_t width, uint32_t height);
void vk_swapchain_destroy(vk_swapchain* swapchain, VkDevice device);

typedef struct vk_api_state {
	vulkan_init_info pf_init_info;
	VkInstance instance;

	VkPhysicalDevice physical_device;
	uint32_t queue_family_idx;
	VkPhysicalDeviceProperties2 device_props;

	VkDevice device;
	VmaAllocator allocator;

	vk_surface surface;

	VkFormat rendertarget_format;
	vk_swapchain swapchain;
} vk_api_state;

// [TODO]: Could be nicer idk
static inline bool check_result(VkResult result) {
	bool ret = result == VK_SUCCESS;
	ASSERT(ret);
	return ret;
}

#endif // !VULKAN_UTIL_H
