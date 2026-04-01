#include "vulkan_internal.h"

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
	ASSERT(out->img_count);

	out->images = vk_image_array_create(arena, out->img_count);
	ASSERT(out->images.raw);
	out->image_views = vk_image_view_array_create(arena, out->img_count);
	ASSERT(out->image_views.raw);

	VkImage* p_images = vk_image_array_begin(out->images);
	VkImageView* p_views = vk_image_view_array_begin(out->image_views);
	ASSERT(p_images);
	ASSERT(p_views);

	if (!check_result(vkGetSwapchainImagesKHR(device, out->swapchain,
											  &out->img_count, p_images))) {
		return false;
	}

	VkImageSubresourceRange view_range = {0};
	{
		view_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_range.levelCount = 1;
		view_range.layerCount = 1;
	}
	for (uint32_t i = 0; i < out->img_count; ++i) {
		VkImageViewCreateInfo viewCI = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = p_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = image_format,
			.subresourceRange = view_range,
		};

		check_result(vkCreateImageView(device, &viewCI, NULL, &p_views[i]));
	}
	return true;
}

bool vk_swapchain_refresh(vk_swapchain* swapchain, VkDevice device,
						  vk_surface* surface, uint32_t width,
						  uint32_t height) {
	VkExtent2D sc_extent = surface->capabilities.currentExtent;
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

	// [TODO]: swapchain image count could change at runtime but will it really?
	if (!check_result(vkGetSwapchainImagesKHR(
			device, swapchain->swapchain, &swapchain->img_count,
			vk_image_array_begin(swapchain->images)))) {
		return false;
	}
	VkImageSubresourceRange view_range = {0};
	{
		view_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_range.levelCount = 1;
		view_range.layerCount = 1;
	}
	for (uint32_t i = 0; i < swapchain->img_count; ++i) {
		VkImage img = VK_NULL_HANDLE;
		ASSERT(vk_image_array_get(swapchain->images, &img, i));
		VkImageViewCreateInfo viewCI = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = img,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchain->img_format,
			.subresourceRange = view_range,
		};

		VkImageView view = VK_NULL_HANDLE;
		ASSERT(vk_image_view_array_get(swapchain->image_views, &view, i));
		check_result(vkCreateImageView(device, &viewCI, NULL, &view));
	}
	return true;
}

void vk_swapchain_destroy(vk_swapchain* swapchain, VkDevice device) {
	ASSERT(swapchain->img_count);
	VkImageView* v = vk_image_view_array_begin(swapchain->image_views);
	ASSERT(v);
	if (v) {
		for (uint32_t i = 0; i < swapchain->img_count; ++i) {
			LOG_DEBUG(
				"vk_swapchain_destroy: destroying swapchain image view %p",
				v[i]);
			vkDestroyImageView(device, v[i], NULL);
		}
	}
	vkDestroySwapchainKHR(device, swapchain->swapchain, NULL);
}
