#include "vulkan_internal.h"

#include <graphics/graphics.h>

bool vk_surface_create(vk_surface* out, VkPhysicalDevice device,
					   VkSurfaceKHR surface) {
	if (!surface) { return false; }
	out->surface = surface;
	return vk_surface_get_capabilities(out, device);
}

bool vk_surface_get_capabilities(vk_surface* surface, VkPhysicalDevice device) {
	return check_result(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		device, surface->surface, &surface->capabilities));
}

void vk_surface_destroy(vk_surface* surface, VkInstance instance) {
	vkDestroySurfaceKHR(instance, surface->surface, NULL);
}

static bool _swapchain_refresh_arrays(vk_swapchain* out, VkDevice device) {
	uint32_t old_img_count = out->img_count;
	if (!check_result(vkGetSwapchainImagesKHR(device, out->swapchain,
											  &out->img_count, NULL))) {
		return false;
	}

	// [TODO]: Does image count really change at runtime?
	ASSERT(out->img_count == old_img_count);

	VkImage* p_images = vk_image_array_begin(out->images);
	VkImageView* p_views = vk_image_view_array_begin(out->image_views);
	VkSemaphore* p_sems = vk_sem_array_begin(out->present_semaphores);
	ASSERT(p_images && p_views && p_sems);

	// First clear old semaphores:
	for (uint32_t i = 0; i < out->img_count; ++i) {
		vkDestroySemaphore(device, p_sems[i], NULL);
	}

	if (!check_result(vkGetSwapchainImagesKHR(device, out->swapchain,
											  &out->img_count, p_images))) {
		return false;
	}

	for (uint32_t i = 0; i < out->img_count; ++i) {
		VkImageViewCreateInfo view_ci = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = p_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = out->img_format,
			.subresourceRange = vk_base_subresource_range(),
		};

		VkSemaphoreCreateInfo sem_ci = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

		check_result(vkCreateImageView(device, &view_ci, NULL, &p_views[i]));
		check_result(vkCreateSemaphore(device, &sem_ci, NULL, &p_sems[i]));
	}

	return true;
}

bool vk_swapchain_create(vk_swapchain* out, arena_t* arena, VkDevice device,
						 vk_surface* surface, uint32_t width, uint32_t height,
						 uint32_t min_image_count, VkFormat image_format) {
	VkExtent2D sc_extent = surface->capabilities.currentExtent;
	if (sc_extent.width == 0xFFFFFFFF) {
		sc_extent = (VkExtent2D){.width = width, .height = height};
	}

	out->ci = (VkSwapchainCreateInfoKHR){
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface->surface,
		.minImageCount = min_image_count,
		.imageFormat = image_format,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = sc_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // TODO: from args
		.presentMode = VK_PRESENT_MODE_FIFO_KHR				 // TODO: from args
	};

	out->img_count = 0;
	out->img_index = 0;
	out->img_format = out->ci.imageFormat;

	if (!check_result(
			vkCreateSwapchainKHR(device, &out->ci, NULL, &out->swapchain))) {
		return false;
	}

	if (!check_result(vkGetSwapchainImagesKHR(device, out->swapchain,
											  &out->img_count, NULL))) {
		return false;
	}

	out->images = vk_image_array_create(arena, out->img_count);
	out->image_views = vk_image_view_array_create(arena, out->img_count);
	out->present_semaphores = vk_sem_array_create(arena, out->img_count);
	ASSERT(out->images.raw && out->image_views.raw &&
		   out->present_semaphores.raw);

	return _swapchain_refresh_arrays(out, device);
}

bool vk_swapchain_refresh(vk_swapchain* swapchain, VkDevice device,
						  vk_surface* surface, uint32_t width,
						  uint32_t height) {
	VkExtent2D sc_extent = surface->capabilities.currentExtent;
	if (sc_extent.height == 0 || sc_extent.height == 0) { return false; }
	if (sc_extent.width == 0xFFFFFFFF) {
		sc_extent = (VkExtent2D){.width = width, .height = height};
	}

	{ // Update CI
		swapchain->ci.surface = surface->surface;
		swapchain->ci.imageExtent = sc_extent;
		swapchain->ci.oldSwapchain = swapchain->swapchain;
	}

	{ // Destroy old swapchain
		check_result(vkDeviceWaitIdle(device));
		vk_swapchain_destroy(swapchain, device);
		swapchain->img_index = 0;
	}

	if (!check_result(vkCreateSwapchainKHR(device, &swapchain->ci, NULL,
										   &swapchain->swapchain))) {
		return false;
	}

	return _swapchain_refresh_arrays(swapchain, device);
}

bool vk_swapchain_get_current_image(vk_swapchain* swapchain, VkImage* out) {
	return vk_image_array_get(swapchain->images, out, swapchain->img_index);
}
bool vk_swapchain_get_current_image_view(vk_swapchain* swapchain,
										 VkImageView* out) {
	return vk_image_view_array_get(swapchain->image_views, out,
								   swapchain->img_index);
}
bool vk_swapchain_get_current_semaphore(vk_swapchain* swapchain,
										VkSemaphore* out) {
	return vk_sem_array_get(swapchain->present_semaphores, out,
							swapchain->img_index);
}

vk_sc_image_acquire_result vk_swapchain_acquire_image(vk_swapchain* swapchain,
													  VkDevice device,
													  vk_rendertarget* out,
													  VkSemaphore semaphore) {
	ASSERT(swapchain->img_index == 0);
	VkResult res =
		vkAcquireNextImageKHR(device, swapchain->swapchain, UINT64_MAX,
							  semaphore, NULL, &swapchain->img_index);

	if (res == VK_ERROR_OUT_OF_DATE_KHR) { return ACQUIRE_OUT_OF_DATE; }
	if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR) {
		return ACQUIRE_ERROR;
	}

	LOG_DEBUG("vk_swapchain_acquire_image: acquired image %u",
			  swapchain->img_index);
	{
		VkImage* imgs_p = vk_image_array_begin(swapchain->images);
		VkImageView* img_views_p =
			vk_image_view_array_begin(swapchain->image_views);
		out->image = imgs_p[swapchain->img_index];
		out->image_view = img_views_p[swapchain->img_index];
		out->extent = swapchain->ci.imageExtent;
	};
	return ACQUIRE_SUCCESS;
}

bool vk_swapchain_present(vk_swapchain* swapchain, VkQueue queue) {
	VkSemaphore wait_semaphore = {0};
	ASSERT(vk_sem_array_get(swapchain->present_semaphores, &wait_semaphore,
							swapchain->img_index));

	LOG_DEBUG("vk_swapchain_present: presenting to image %u",
			  swapchain->img_index);
	VkPresentInfoKHR present_info = {0};
	{
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &wait_semaphore;
		present_info.pImageIndices = &swapchain->img_index;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &swapchain->swapchain;
	}

	// [TODO]: check this result as it can fail when out of date
	vkQueuePresentKHR(queue, &present_info);

	swapchain->img_index = 0;

	return true;
}

void vk_swapchain_destroy(vk_swapchain* swapchain, VkDevice device) {
	ASSERT(swapchain->img_count);
	VkImageView* v = vk_image_view_array_begin(swapchain->image_views);
	VkSemaphore* s = vk_sem_array_begin(swapchain->present_semaphores);
	ASSERT(v);
	if (v) {
		for (uint32_t i = 0; i < swapchain->img_count; ++i) {
			vkDestroyImageView(device, v[i], NULL);
			vkDestroySemaphore(device, s[i], NULL);
		}
	}
	vkDestroySwapchainKHR(device, swapchain->swapchain, NULL);
}

bool vk_render_sync_create(VkDevice device, vk_render_sync* out,
						   VkCommandPool command_pool) {
	VkSemaphoreCreateInfo sem_ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

	// Create fences signalled since there's nothing to wait for on 1st frame
	VkFenceCreateInfo fence_ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	if (!check_result(
			vkCreateSemaphore(device, &sem_ci, NULL, &out->draw_sem))) {
		return false;
	}
	if (!check_result(
			vkCreateFence(device, &fence_ci, NULL, &out->drawn_fence))) {
		return false;
	}

	VkCommandBufferAllocateInfo alloc_info = {0};
	{
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandBufferCount = 1;
		alloc_info.commandPool = command_pool;
		alloc_info.level = 0;
	}

	if (!check_result(vkAllocateCommandBuffers(device, &alloc_info,
											   &out->command_buffer))) {
		return false;
	}

	return true;
}

void vk_render_sync_destroy(VkDevice device, vk_render_sync* sync) {
	vkDestroySemaphore(device, sync->draw_sem, NULL);
	vkDestroyFence(device, sync->drawn_fence, NULL);
	// Command buffer will be destroyed by the pool
}

bool vk_sync_buffer_create(VkDevice device, vk_sync_buffer* out,
						   uint32_t frames_in_flight,
						   VkCommandPool command_pool) {
	out->frames_in_flight = frames_in_flight;
	for (uint32_t i = 0; i < out->frames_in_flight; ++i) {
		if (!vk_render_sync_create(device, &out->frames[i], command_pool)) {
			return false;
		}
	}
	return true;
}

void vk_sync_buffer_destroy(VkDevice device, vk_sync_buffer* out) {
	for (uint32_t i = 0; i < out->frames_in_flight; ++i) {
		vk_render_sync_destroy(device, &out->frames[i]);
	}
}

VkImageSubresourceRange vk_base_subresource_range(void) {
	static VkImageSubresourceRange const view_range = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.levelCount = 1,
		.layerCount = 1,
	};
	return view_range;
}

VkFormat to_vk_format(gpu_image_format f) {
	switch (f) {
	case IMAGE_FORMAT_RGBA_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
	case IMAGE_FORMAT_RGBA_SNORM: return VK_FORMAT_R8G8B8A8_SNORM;
	case IMAGE_FORMAT_RGBA_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;

	case IMAGE_FORMAT_RGB_UNORM: return VK_FORMAT_R8G8B8_UNORM;
	case IMAGE_FORMAT_RGB_SNORM: return VK_FORMAT_R8G8B8_SNORM;
	case IMAGE_FORMAT_RGB_SRGB: return VK_FORMAT_R8G8B8_SRGB;
	default: ASSERT(false); return VK_FORMAT_R8G8B8_UNORM;
	}
}
