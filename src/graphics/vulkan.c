#ifndef GRAPHICS_VULKAN_H
#define GRAPHICS_VULKAN_H

#include "vulkan_init_info.h"

#include <containers/array.h>
#include <core/core.h>
#include <graphics/graphics.h>

#include <vulkan/vulkan.h>

ARRAY_DEFINE_TYPE(VkPhysicalDevice, physical_device_array)

// [TODO]: Could be nicer idk
static inline bool check_result(VkResult result) {
	bool ret = result == VK_SUCCESS;
	ASSERT(ret);
	return ret;
}

typedef struct vk_api_state {
	VkInstance instance;
	VkPhysicalDevice physical_device;
} vk_api_state;

static VkPhysicalDevice vk_find_device(VkInstance instance, arena_t* scratch) {
	uint32_t device_count = 0;
	check_result(vkEnumeratePhysicalDevices(instance, &device_count, NULL));

	physical_device_array arr = physical_device_array_create(scratch, 16);
	ASSERT(arr.raw);
	check_result(vkEnumeratePhysicalDevices(instance, &device_count,
											physical_device_array_begin(arr)));

	LOG_DEBUG("vulkan_initialize: %zu physical devices", device_count);

	VkPhysicalDevice* dev = physical_device_array_begin(arr);
	VkPhysicalDevice* fallback = NULL;
	VkPhysicalDeviceProperties2 device_props = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	size_t i = 0;
	for (; i < device_count; ++i) {

		vkGetPhysicalDeviceProperties2(*dev, &device_props);
		if (device_props.properties.deviceType ==
			VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			break;
		}
		if (device_props.properties.deviceType ==
			VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
			fallback = dev;
		}
		dev++;
	}

	if (i == device_count) {
		if (!fallback) {
			LOG_ERR("vulkan_initialize: no suitable GPU found");
			return NULL;
		}
		dev = fallback;
	}

	// Get props again in case we fallback on earlier device
	vkGetPhysicalDeviceProperties2(*dev, &device_props);
	LOG_DEBUG("vulkan_initialize: physical device found: '%s'",
			  device_props.properties.deviceName);

	return *dev;
}

void vulkan_initialize(gpu_api* self, gpu_api_platform_init_info* platform_info,
					   arena_t* arena, arena_t* scratch) {
	if (!self || !arena || !scratch || !platform_info) {
		LOG_ERR("vulkan_initialize: invalid parameters");
		return;
	}

	vk_api_state* state = arena_push(arena, vk_api_state);
	if (!state) {
		LOG_ERR("vulkan_initialize: couldn't allocate state");
		ASSERT(false);
		return;
	}

	// Instance:
	VkInstance instance;
	{
		VkApplicationInfo app_ci = {0};
		{
			app_ci.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			app_ci.pApplicationName = self->name;
			app_ci.apiVersion = VK_API_VERSION_1_4;
		}

		vulkan_init_info* vk_init_info = (vulkan_init_info*)platform_info;
		ASSERT(vk_init_info->required_instance_extensions_count);

		VkInstanceCreateInfo instance_ci = {0};
		{
			instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instance_ci.pApplicationInfo = &app_ci;
			instance_ci.enabledExtensionCount =
				vk_init_info->required_instance_extensions_count;
			instance_ci.ppEnabledExtensionNames =
				vk_init_info->required_instance_extensions;
		}

		check_result(vkCreateInstance(&instance_ci, NULL, &instance));
		ASSERT(instance);
	}

	// Device:
	VkPhysicalDevice dev = vk_find_device(instance, scratch);
	ASSERT(dev);

	state->instance = instance;
	state->physical_device = dev;

	{
		self->is_init = true;
		self->state = state;
	}
}

bool gpu_create_api(gpu_api_config config, gpu_api* out, arena_t* scratch) {
	if (!out || !scratch) {
		LOG_ERR("gpu_create_api: invalid parameters");
		return false;
	}
	ASSERT(config.type == GRAPHICS_API_VULKAN);

	{
		out->type = GRAPHICS_API_VULKAN;
		out->is_init = false;
		out->name = config.app_name;
		out->initialize = vulkan_initialize;
	}

	return true;
}

#endif // !GRAPHICS_VULKAN_H
