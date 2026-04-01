#ifndef GRAPHICS_VULKAN_H
#define GRAPHICS_VULKAN_H

#include <vulkan/vulkan.h>

#include "vulkan_init_info.h"
#include "vulkan_internal.h"

#include <containers/array.h>
#include <core/core.h>
#include <graphics/graphics.h>

#include <string.h>
#include <vma/vk_mem_alloc.h>

ARRAY_DEFINE_TYPE(VkPhysicalDevice, vk_physical_device_array)
ARRAY_DEFINE_TYPE(VkQueueFamilyProperties2, vk_qf_props_array)
ARRAY_DEFINE_TYPE(VkExtensionProperties, vk_ext_props_array)

static VkFormat to_vk_format(gpu_image_format f) {
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

	vk_ext_props_array arr = vk_ext_props_array_create(scratch, ext_count);
	assert(arr.raw);

	VkExtensionProperties* props = vk_ext_props_array_begin(arr);
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

// Returns index of the first queue family with graphics and compute
// capabilities
static uint32_t vk_get_queue_family(VkPhysicalDevice dev, arena_t* scratch) {

	// Get array size first:
	uint32_t qf_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(dev, &qf_count, NULL);

	if (!qf_count) { return UINT32_MAX; }
	LOG_DEBUG("vulkan_initialize: enumerating %zu qf properties", qf_count);

	// Create array with the right size:
	vk_qf_props_array props_arr = vk_qf_props_array_create(scratch, qf_count);
	ASSERT(props_arr.raw);
	VkQueueFamilyProperties2* qf_props = vk_qf_props_array_begin(props_arr);

	// Validation layers complain if sType aren't initialized:
	for (size_t i = 0; i < qf_count; ++i) {
		qf_props[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
	}

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
static vk_find_device_result vk_find_device(vulkan_init_info* pf_info,
											VkInstance instance,
											arena_t* scratch) {
	vk_find_device_result ret = {0};
	uint32_t device_count = 0;
	check_result(vkEnumeratePhysicalDevices(instance, &device_count, NULL));

	vk_physical_device_array arr = vk_physical_device_array_create(scratch, 16);
	if (!arr.raw) { return ret; }
	check_result(vkEnumeratePhysicalDevices(
		instance, &device_count, vk_physical_device_array_begin(arr)));

	LOG_DEBUG("vulkan_initialize: %zu physical devices", device_count);

	VkPhysicalDevice* dev = vk_physical_device_array_begin(arr);
	VkPhysicalDeviceProperties2 device_props = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	size_t i = 0;
	for (; i < device_count; ++i) {

		vkGetPhysicalDeviceProperties2(dev[i], &device_props);
		if (!is_dgpu(device_props) && !is_igpu(device_props)) { continue; }

		if (!is_vk_13(device_props)) { continue; }
		uint32_t qf = vk_get_queue_family(dev[i], scratch);
		if (qf == UINT32_MAX) { continue; }

		if (!pf_info->can_device_queue_present(instance, dev[i], qf) ||
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

VkDevice vk_create_device(VkPhysicalDevice physical_device,
						  uint32_t queue_idx) {
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
		queue_ci.queueFamilyIndex = queue_idx;
		queue_ci.queueCount = 1;
		queue_ci.pQueuePriorities = &qfpriorities;
	}

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

	VkDevice device = NULL;
	check_result(vkCreateDevice(physical_device, &device_ci, NULL, &device));
	return device;
}

void vulkan_initialize(gpu_api* self, window_t* window, arena_t* arena,
					   arena_t* scratch) {
	if (!self || !arena || !scratch) {
		LOG_ERR("vulkan_initialize: invalid parameters");
		return;
	}

	vk_api_state* state = arena_push(arena, vk_api_state);
	if (!state) {
		LOG_ERR("vulkan_initialize: couldn't allocate state");
		ASSERT(false);
		return;
	}

	// Auto-zeroing arena can be toggled off, so let's be pedantic
	memset(state, 0, sizeof(*state));

	{
		vulkan_init_info vk_pf_info = {0};
		if (!platform_init_graphics_api(
				GRAPHICS_API_VULKAN,
				(gpu_api_platform_init_info*)&vk_pf_info)) {
			LOG_ERR("vulkan_initialize: couldn't init platform for vulkan");
			ASSERT(false);
			return;
		};
		ASSERT(vk_pf_info.required_instance_extensions_count);
		state->pf_init_info = vk_pf_info;
	}

	// Instance:
	{
		VkInstance instance;
		VkApplicationInfo app_ci = {0};
		{
			app_ci.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			app_ci.pApplicationName = self->name;
			app_ci.apiVersion = VK_API_VERSION_1_4;
		}

		VkInstanceCreateInfo instance_ci = {0};
		{
			instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instance_ci.pApplicationInfo = &app_ci;
			instance_ci.enabledExtensionCount =
				state->pf_init_info.required_instance_extensions_count;
			instance_ci.ppEnabledExtensionNames =
				state->pf_init_info.required_instance_extensions;
		}

		check_result(vkCreateInstance(&instance_ci, NULL, &instance));
		ASSERT(instance);
		state->instance = instance;
	}

	// Physical Device:
	{
		vk_find_device_result dev_result =
			vk_find_device(&state->pf_init_info, state->instance, scratch);
		if (!dev_result.found) {
			ASSERT(false);
			return;
		}
		state->physical_device = dev_result.device;
		state->queue_family_idx = dev_result.queue_index;
		state->device_props = dev_result.device_props;
	}

	// Logical Device:
	{
		VkDevice device =
			vk_create_device(state->physical_device, state->queue_family_idx);
		if (!device) {
			ASSERT(false);
			return;
		}
		state->device = device;
	}

	// VMA:
	{
		VmaVulkanFunctions vk_funcs = {
			.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
			.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
			.vkCreateImage = vkCreateImage,
		};
		VmaAllocatorCreateInfo alloc_ci = {
			.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
			.physicalDevice = state->physical_device,
			.device = state->device,
			.pVulkanFunctions = &vk_funcs,
			.instance = state->instance,
		};

		VmaAllocator allocator;
		check_result(vmaCreateAllocator(&alloc_ci, &allocator));
		state->allocator = allocator;
	}

	// Surface:
	{
		VkSurfaceKHR surface =
			state->pf_init_info.create_surface(window, state->instance);
		if (!vk_surface_create(&state->surface, state->physical_device,
							   surface)) {
			LOG_ERR("vulkan_initialize: couldn't create surface");
			return;
		}
	}

	// Swapchain:
	{
		window_size win_size = platform_get_window_size(window);
		state->rendertarget_format =
			to_vk_format(self->config.target_image_format);

		// Buffering setup:
		uint32_t buffering = self->config.frames_in_flight;
		uint32_t sc_min = state->surface.capabilities.minImageCount;
		uint32_t sc_max = state->surface.capabilities.maxImageCount;

		if (sc_max < buffering) {
			LOG_ERR("vulkan_initialize: required frames_in_flight is above "
					"swapchain buffering limit");
			return;
		}
		// [NOTE]: More images than buffering is allowed. We can double-buffer
		// with 4 swapchain images, no problem.

		uint32_t sc_img_count = CLAMP(buffering, sc_min, sc_max);

		if (!vk_swapchain_create(&state->swapchain, arena, state->device,
								 &state->surface, win_size.width,
								 win_size.height, sc_img_count,
								 state->rendertarget_format)) {
			LOG_ERR("vulkan_initialize: couldn't create swapchain");
			return;
		}
		LOG_DEBUG("vulkan_initialize: created swapchain with %u images",
				  state->swapchain.img_count);
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

void vulkan_destroy(struct gpu_api* self) {
	if (!self) {
		LOG_ERR("vulkan_destroy: invalid instance");
		return;
	}
	if (!self->is_init || !self->state) { return; }

	vk_api_state* state = self->state;

	{
		if (state->device) {
			vk_swapchain_destroy(&state->swapchain, state->device);
			vk_surface_destroy(&state->surface, state->instance);
			vmaDestroyAllocator(state->allocator);
			vkDestroyDevice(state->device, NULL);
		}
		vkDestroyInstance(state->instance, NULL);
	}
	LOG_DEBUG("vulkan_destroy: destroyed vulkan api");
}

bool gpu_create_api(gpu_api_config config, gpu_api* out) {
	if (!out) {
		LOG_ERR("gpu_create_api: invalid parameters");
		return false;
	}
	ASSERT(config.type == GRAPHICS_API_VULKAN);

	{
		out->type = GRAPHICS_API_VULKAN;
		out->is_init = false;
		out->name = config.app_name;
		out->initialize = vulkan_initialize;
		out->destroy = vulkan_destroy;
	}

	return true;
}

#endif // !GRAPHICS_VULKAN_H
