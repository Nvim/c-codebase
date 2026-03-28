#ifndef GRAPHICS_VULKAN_H
#define GRAPHICS_VULKAN_H

#include <vulkan/vulkan.h>

#include "vulkan_init_info.h"

#include <containers/array.h>
#include <core/core.h>
#include <graphics/graphics.h>

#include <GLFW/glfw3.h>
#include <string.h>

ARRAY_DEFINE_TYPE(VkPhysicalDevice, physical_device_array)
ARRAY_DEFINE_TYPE(VkQueueFamilyProperties2, qf_props_array)
ARRAY_DEFINE_TYPE(VkExtensionProperties, ext_props_array)

// [TODO]: Could be nicer idk
static inline bool check_result(VkResult result) {
	bool ret = result == VK_SUCCESS;
	ASSERT(ret);
	return ret;
}

typedef struct vk_api_state {
	VkInstance instance;

	VkPhysicalDevice physical_device;
	uint32_t queue_family_idx;
	VkPhysicalDeviceProperties2 device_props;

	VkDevice device;

} vk_api_state;

static bool is_vk_13(VkPhysicalDeviceProperties2 props) {
	return props.properties.apiVersion >= VK_API_VERSION_1_3;
}
static bool is_dgpu(VkPhysicalDeviceProperties2 props) {
	return props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}
static bool is_igpu(VkPhysicalDeviceProperties2 props) {
	return props.properties.deviceType ==
		   VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

// Only look for swapchain for now
static bool has_device_exts(VkPhysicalDevice dev, arena_t* scratch) {
	uint32_t ext_count;
	check_result(
		vkEnumerateDeviceExtensionProperties(dev, NULL, &ext_count, NULL));

	ext_props_array arr = ext_props_array_create(scratch, ext_count);
	assert(arr.raw);

	VkExtensionProperties* props = ext_props_array_begin(arr);
	check_result(
		vkEnumerateDeviceExtensionProperties(dev, NULL, &ext_count, props));

	for (uint32_t i = 0; i < ext_count; i++) {
		if (strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, props[i].extensionName) ==
			0) {
			return true;
		}
	}
	return false;
}

static bool can_present(VkInstance instance, VkPhysicalDevice dev,
						uint32_t queue_family_idx) {
	return glfwGetPhysicalDevicePresentationSupport(
			   instance, dev, queue_family_idx) == GLFW_TRUE;
}

// Returns index of the first queue family with graphics and compute
// capabilities
static uint32_t vk_get_queue_family(VkPhysicalDevice dev, arena_t* scratch) {

	// Get array size first:
	uint32_t qf_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(dev, &qf_count, NULL);

	if (!qf_count) { return UINT32_MAX; }
	LOG_DEBUG("vulkan_initialize: enumerating %zu qf properties", qf_count);

	// Create array with the right size:
	qf_props_array props_arr = qf_props_array_create(scratch, qf_count);
	ASSERT(props_arr.raw);
	VkQueueFamilyProperties2* qf_props = qf_props_array_begin(props_arr);

	// Get the props:
	vkGetPhysicalDeviceQueueFamilyProperties2(dev, &qf_count, qf_props);

	for (size_t i = 0; i < qf_count; ++i) {
		VkQueueFlags flags = qf_props[i].queueFamilyProperties.queueFlags;
		bool has_graphics = (flags & VK_QUEUE_GRAPHICS_BIT);
		bool has_computue =
			qf_props[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;

		if (has_graphics && has_computue) { return (uint32_t)i; }
	}
	return UINT32_MAX;
}

typedef struct vk_find_device_result {
	bool found;
	VkPhysicalDevice device;
	uint32_t queue_index;
	VkPhysicalDeviceProperties2 device_props;
} vk_find_device_result;

/**
 * @brief Find a Physical device with the following:
 *   - Vulkan 1.3+ support
 *   - A queue that supports both graphics & compute
 *   - Swaphchain extension
 *   - Can present
 *
 * The first dedicated GPU with all the above properties will be returned, or
 * fallback to an iGPU. If none if available, fails
 *
 */
static vk_find_device_result vk_find_device(VkInstance instance,
											arena_t* scratch) {
	vk_find_device_result ret = {0};
	uint32_t device_count = 0;
	check_result(vkEnumeratePhysicalDevices(instance, &device_count, NULL));

	physical_device_array arr = physical_device_array_create(scratch, 16);
	if (!arr.raw) { return ret; }
	check_result(vkEnumeratePhysicalDevices(instance, &device_count,
											physical_device_array_begin(arr)));

	LOG_DEBUG("vulkan_initialize: %zu physical devices", device_count);

	VkPhysicalDevice* dev = physical_device_array_begin(arr);
	VkPhysicalDeviceProperties2 device_props = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	size_t i = 0;
	for (; i < device_count; ++i) {

		vkGetPhysicalDeviceProperties2(dev[i], &device_props);
		if (!is_dgpu(device_props) && !is_igpu(device_props)) { continue; }

		if (!is_vk_13(device_props)) { continue; }
		uint32_t qf = vk_get_queue_family(dev[i], scratch);
		if (qf == UINT32_MAX) { continue; }

		if (!can_present(instance, dev[i], qf) ||
			!has_device_exts(dev[i], scratch)) {
			LOG_DEBUG("device `%s` can't present",
					  device_props.properties.deviceName);
			continue;
		}

		{
			ret.device = dev[i];
			ret.queue_index = qf;
			ret.device_props = device_props;
			ret.found = true;
		}
		if (is_dgpu(device_props)) { return ret; }
		// Keep searching in case we get dgpu later
	}

	if (i == device_count && !ret.found) {
		LOG_ERR("vulkan_initialize: no suitable GPU found");
		memset(&ret, 0, sizeof(ret));
		return ret;
	}

	// Get props again in case we fallback on earlier device
	vkGetPhysicalDeviceProperties2(ret.device, &ret.device_props);
	return ret;
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

	// Physical Device:
	vk_find_device_result dev_result = vk_find_device(instance, scratch);
	if (!dev_result.found) {
		ASSERT(false);
		return;
	}

	// Logical Device:
	VkPhysicalDeviceVulkan12Features vk_12_features = {0};
	{
		vk_12_features.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vk_12_features.descriptorIndexing = true;
		vk_12_features.shaderSampledImageArrayNonUniformIndexing = true;
		vk_12_features.descriptorBindingVariableDescriptorCount = true;
		vk_12_features.runtimeDescriptorArray = true;
		vk_12_features.bufferDeviceAddress = true;
	}
	VkPhysicalDeviceVulkan13Features vk_13_features = {0};
	{
		vk_13_features.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vk_13_features.pNext = &vk_12_features;
		vk_13_features.synchronization2 = true;
		vk_13_features.dynamicRendering = true;
	}
	VkPhysicalDeviceFeatures vk_10_features = {.samplerAnisotropy = VK_TRUE};

	static char const* required_exts[1] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	static float const qfpriorities = 1.0f;

	VkDeviceQueueCreateInfo queue_ci = {0};
	{
		queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_ci.queueFamilyIndex = dev_result.queue_index;
		queue_ci.queueCount = 1;
		queue_ci.pQueuePriorities = &qfpriorities;
	}

	VkDevice device;
	VkDeviceCreateInfo device_ci = {0};
	{
		device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_ci.pNext = &vk_13_features;
		device_ci.queueCreateInfoCount = 1;
		device_ci.pQueueCreateInfos = &queue_ci;
		device_ci.enabledExtensionCount = 1;
		device_ci.ppEnabledExtensionNames = required_exts;
		device_ci.pEnabledFeatures = &vk_10_features;
	}
	check_result(vkCreateDevice(dev_result.device, &device_ci, NULL, &device));

	{ // Setup state
		state->instance = instance;
		state->physical_device = dev_result.device;
		state->queue_family_idx = dev_result.queue_index;
		state->device_props = dev_result.device_props;
		state->device = device;
	}

	{
		self->is_init = true;
		self->state = state;
	}
	LOG_DEBUG("vulkan_initialize: Using device: '%s'",
			  state->device_props.properties.deviceName);
	LOG_DEBUG("vulkan_initialize: Using queue family: '%u'",
			  state->queue_family_idx);
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
