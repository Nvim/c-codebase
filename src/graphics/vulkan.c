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

#define IMAGE_POOL_SIZE 1024
#define CMDBUF_POOL_SIZE 1024

static bool _is_vk_13(VkPhysicalDeviceProperties2 props) {
	return props.properties.apiVersion >= VK_API_VERSION_1_3;
}
static bool _is_dgpu(VkPhysicalDeviceProperties2 props) {
	return props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}
static bool _is_igpu(VkPhysicalDeviceProperties2 props) {
	return props.properties.deviceType ==
		   VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

// Only look for swapchain for now
static bool _device_exts(VkPhysicalDevice dev, arena_t* scratch) {
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
static uint32_t _get_queue_family(VkPhysicalDevice dev, arena_t* scratch) {

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
static vk_find_device_result
_find_device(vulkan_init_info* pf_info, VkInstance instance, arena_t* scratch) {
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
		if (!_is_dgpu(device_props) && !_is_igpu(device_props)) { continue; }

		if (!_is_vk_13(device_props)) { continue; }
		uint32_t qf = _get_queue_family(dev[i], scratch);
		if (qf == UINT32_MAX) { continue; }

		if (!pf_info->can_device_queue_present(instance, dev[i], qf) ||
			!_device_exts(dev[i], scratch)) {
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
		if (_is_dgpu(device_props)) { return ret; }
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

static VkDevice _create_device(VkPhysicalDevice physical_device,
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

static bool validate_gpu_config(gpu_api_config* config) {
	if (!config->frames_in_flight ||
		config->frames_in_flight > VK_MAX_BUFFERING) {
		LOG_ERR("validate_gpu_config: invalid buffering amount");
		return false;
	}
	return true;
}

void vulkan_initialize(gpu_api* self, window_t* window, arena_t* arena,
					   arena_t* scratch) {
	if (!self || !arena || !scratch || !window) {
		LOG_ERR("vulkan_initialize: invalid parameters");
		return;
	}

	if (!validate_gpu_config(&self->config)) { return; }

	self->window = window;
	vk_api_state* state = arena_push_z(arena, vk_api_state);
	if (!state) {
		LOG_ERR("vulkan_initialize: couldn't allocate state");
		ASSERT(false);
		return;
	}

	// Init global pools:
	_image_pool = vk_image_pool_create(arena, IMAGE_POOL_SIZE);
	_cmd_pool = vk_cmdbuf_pool_create(arena, CMDBUF_POOL_SIZE);

	ASSERT(_image_pool.raw && _cmd_pool.raw);

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

	{ // Instance:
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

	{ // Physical Device:
		vk_find_device_result dev_result =
			_find_device(&state->pf_init_info, state->instance, scratch);
		if (!dev_result.found) {
			ASSERT(false);
			return;
		}
		state->physical_device = dev_result.device;
		state->queue_family_idx = dev_result.queue_index;
		state->device_props = dev_result.device_props;
	}

	{ // Logical Device:
		VkDevice device =
			_create_device(state->physical_device, state->queue_family_idx);
		if (!device) {
			ASSERT(false);
			return;
		}
		state->device = device;

		vkGetDeviceQueue(device, state->queue_family_idx, 0, &state->queue);
		ASSERT(state->queue);
	}

	state->frames_in_flight = self->config.frames_in_flight;

	{ // VMA:
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
		ASSERT(surface);
		if (!vk_surface_create(&state->surface, state->physical_device,
							   surface)) {
			LOG_ERR("vulkan_initialize: couldn't create surface");
			return;
		}
		ASSERT(state->surface.capabilities.minImageCount);
	}

	// Swapchain:
	{
		window_size win_size = platform_get_window_size(window);
		state->rendertarget_format =
			gpu_to_vk_image_format(self->config.target_image_format);

		// Buffering setup:
		uint32_t buffering = self->config.frames_in_flight;
		ASSERT(buffering);
		uint32_t sc_min = state->surface.capabilities.minImageCount;
		uint32_t sc_max = state->surface.capabilities.maxImageCount;

		// capabilities.maxImageCount can be 0 (unlimited)
		if (sc_max == 0) { sc_max = buffering; }

		if (sc_max < buffering) {
			LOG_ERR("vulkan_initialize: required frames_in_flight is above "
					"swapchain buffering limit (%u < %u)",
					sc_max, buffering);
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

	{ // Frame sync
		VkCommandPoolCreateInfo pool_ci = {0};
		{
			pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			pool_ci.queueFamilyIndex = state->queue_family_idx;
			pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		}
		check_result(vkCreateCommandPool(state->device, &pool_ci, NULL,
										 &state->command_pool));
		if (!vk_sync_buffer_create(state->device, &state->sync_buffer,
								   self->config.frames_in_flight,
								   state->command_pool)) {
			LOG_FATAL("vulkan_initialize: couldn't create rendersync");
			return;
		}
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

// void vulkan_render(struct gpu_api* self) {
// 	// render is called in a (very) hot loop, no validation here
// 	vk_api_state* state = self->state;
// 	vk_render_sync frame_sync =
// 		state->sync_buffer
// 			.frames[state->frame_idx % self->config.frames_in_flight];
//
// 	static uint64_t const fence_timeout = sec_to_ns(1);
// 	vkWaitForFences(state->device, 1, &frame_sync.drawn_fence, false,
// 					fence_timeout);
// 	vkResetFences(state->device, 1, &frame_sync.drawn_fence);
//
// 	vk_rendertarget rendertarget;
// 	{ // Acquire a swapchain image
// 		LOG_WARN("vulkan_render: acquiring rendertarget");
// 		vk_sc_image_acquire_result ret =
// 			vk_swapchain_acquire_image(&state->swapchain, state->device,
// 									   &rendertarget, frame_sync.draw_sem);
//
// 		// [TODO]: don't just crash on this
// 		ASSERT(ret != ACQUIRE_ERROR);
//
// 		if (ret == ACQUIRE_OUT_OF_DATE) {
// 			LOG_INFO("vulkan_render: handling out of date swapchain");
// 			window_size win_size = platform_get_window_size(self->window);
// 			bool ok = vk_swapchain_refresh(&state->swapchain, state->device,
// 										   &state->surface, win_size.width,
// 										   win_size.height);
// 			ASSERT(ok); // [TODO]: don't crash
// 		}
// 	}
//
// 	VkSemaphore sc_sem;
// 	ASSERT(vk_swapchain_get_current_semaphore(&state->swapchain, &sc_sem));
//
// 	VkCommandBuffer cmd_buf = frame_sync.command_buffer;
// 	{ // Begin command buffer
// 		// check_result(vkResetCommandBuffer(cmd_buf, 0));
// 		LOG_WARN("vulkan_render: beginning command buffer");
// 		VkCommandBufferBeginInfo begin_info = {0};
// 		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
// 		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//
// 		// [TODO]: handle in release too
// 		check_result(vkBeginCommandBuffer(cmd_buf, &begin_info));
// 	}
//
// 	{ // Transition swapchain image UNDEFINED -> ATTACHMENT_OPTIMAL
// 		VkImageMemoryBarrier2 barrier = {0};
// 		{
// 			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
// 			barrier.image = rendertarget.image;
// 			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
// 			barrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
// 			barrier.srcStageMask =
// 				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
// 			barrier.dstStageMask =
// 				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
// 			barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
// 									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
// 			barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
// 									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
// 			barrier.subresourceRange = vk_base_subresource_range();
// 			barrier.srcQueueFamilyIndex = state->queue_family_idx;
// 			barrier.dstQueueFamilyIndex = state->queue_family_idx;
// 		}
//
// 		VkDependencyInfo dep_info = {0};
// 		{
// 			dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
// 			dep_info.imageMemoryBarrierCount = 1;
// 			dep_info.pImageMemoryBarriers = &barrier;
// 		}
// 		vkCmdPipelineBarrier2(cmd_buf, &dep_info);
// 	}
//
// 	{ // Actually draw:
// 		LOG_WARN("vulkan_render: rendering");
//
// 		VkRenderingAttachmentInfo color_attachment = {0};
// 		VkClearValue color = {.color = {.float32 = {1.f, 0.f, 1.f, 1.f}}};
// 		{
// 			color_attachment.sType =
// 				VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
// 			color_attachment.imageView = rendertarget.image_view;
// 			color_attachment.clearValue = color;
// 			color_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
// 			color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
// 			color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
// 		}
// 		VkRect2D rect = {.extent = rendertarget.extent,
// 						 .offset = (VkOffset2D){.x = 0, .y = 0}};
// 		VkRenderingInfo rendering_info = {0};
// 		{
// 			rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
// 			rendering_info.renderArea = rect;
// 			rendering_info.colorAttachmentCount = 1;
// 			rendering_info.pColorAttachments = &color_attachment;
// 			rendering_info.layerCount = 1;
// 		}
// 		vkCmdBeginRendering(cmd_buf, &rendering_info);
// 		vkCmdEndRendering(cmd_buf);
// 	}
//
// 	{ // Transition swapchain image ATTACHMENT_OPTIMAL -> PRESENT_SRC
// 		VkImageMemoryBarrier2 barrier = {0};
// 		{
// 			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
// 			barrier.image = rendertarget.image;
// 			barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
// 			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
// 			barrier.srcStageMask =
// 				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
// 			barrier.dstStageMask =
// 				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
// 			barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
// 									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
// 			barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
// 									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
// 			barrier.subresourceRange = vk_base_subresource_range();
// 			barrier.srcQueueFamilyIndex = state->queue_family_idx;
// 			barrier.dstQueueFamilyIndex = state->queue_family_idx;
// 		}
//
// 		VkDependencyInfo dep_info = {0};
// 		{
// 			dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
// 			dep_info.imageMemoryBarrierCount = 1;
// 			dep_info.pImageMemoryBarriers = &barrier;
// 		}
// 		vkCmdPipelineBarrier2(cmd_buf, &dep_info);
// 	}
//
// 	{ // Submit:
// 		LOG_WARN("vulkan_render: submitting");
// 		check_result(vkEndCommandBuffer(cmd_buf));
// 		VkCommandBufferSubmitInfo cmd_info = {0};
// 		{
// 			cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
// 			cmd_info.commandBuffer = cmd_buf;
// 			cmd_info.deviceMask = 0;
// 		}
//
// 		VkSemaphoreSubmitInfo signal_sem_info = {0};
// 		{
// 			signal_sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
// 			signal_sem_info.pNext = NULL;
// 			signal_sem_info.semaphore = sc_sem;
// 			signal_sem_info.stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
// 			signal_sem_info.deviceIndex = 0;
// 			signal_sem_info.value = 1;
// 		}
//
// 		VkSemaphoreSubmitInfo wait_sem_info = {0};
// 		{
// 			wait_sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
// 			wait_sem_info.pNext = NULL;
// 			wait_sem_info.semaphore = frame_sync.draw_sem;
// 			wait_sem_info.stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
// 			wait_sem_info.deviceIndex = 0;
// 			wait_sem_info.value = 1;
// 		}
//
// 		VkSubmitInfo2 submit_info = {0};
// 		{
// 			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
// 			submit_info.commandBufferInfoCount = 1;
// 			submit_info.pCommandBufferInfos = &cmd_info;
// 			submit_info.signalSemaphoreInfoCount = 1;
// 			submit_info.pSignalSemaphoreInfos = &signal_sem_info;
// 			submit_info.waitSemaphoreInfoCount = 1;
// 			submit_info.pWaitSemaphoreInfos = &wait_sem_info;
// 		}
// 		check_result(vkQueueSubmit2(state->queue, 1, &submit_info,
// 									frame_sync.drawn_fence));
// 	}
//
// 	LOG_WARN("vulkan_render: presenting");
// 	ASSERT(vk_swapchain_present(&state->swapchain, state->queue));
//
// 	state->frame_idx++;
// }

gpu_frame_result vulkan_begin_frame(struct gpu_api* self) {
	gpu_frame_result ret = {0};

	vk_api_state* state = self->state;
	window_size win_size = platform_get_window_size(self->window);
	vk_frame frame = {0};
	if (!vk_begin_frame(state, &frame, win_size)) {
		LOG_ERR("vulkan_begin_frame: couldn't begin frame");
		return ret;
	}

	VkCommandBufferBeginInfo begin_info = {0};
	{
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}

	vk_cmdbuf_data* cmd_data =
		vk_command_buffer_get(frame.frame_sync->command_buffer);
	if (!check_result(
			vkBeginCommandBuffer(cmd_data->command_buffer, &begin_info))) {
		return ret;
	}

	ret.frame = vk_to_gpu_frame(&frame);
	ret.ok = true;
	return ret;
}

void vulkan_cmd_begin_rendering(struct gpu_api* self, gpu_command_buffer cmd,
								gpu_render_state* s) {
	vk_api_state* state = self->state;
	vk_cmdbuf_pool_handle vk_handle = {.raw_handle.handle = cmd.handle};
	VkCommandBuffer cmd_buf = vk_command_buffer_get(vk_handle)->command_buffer;

	{ // [TODO]: Hardcoded UNDEFINED -> ATTACHMENT_OPTIMAL barrier
		// vk_image_data* img_data = vk_image_get(s->color_targets[0]);
		VkImage sc_img;
		ASSERT(vk_swapchain_get_current_image(&state->swapchain, &sc_img));
		VkImageMemoryBarrier2 barrier = {0};
		{
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			barrier.image = sc_img;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
			barrier.srcStageMask =
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstStageMask =
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.subresourceRange = vk_base_subresource_range();
			barrier.srcQueueFamilyIndex = state->queue_family_idx;
			barrier.dstQueueFamilyIndex = state->queue_family_idx;
		}

		VkDependencyInfo dep_info = {0};
		{
			dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			dep_info.imageMemoryBarrierCount = 1;
			dep_info.pImageMemoryBarriers = &barrier;
		}
		vkCmdPipelineBarrier2(cmd_buf, &dep_info);
	}

	VkRenderingInfo rendering_info = {0};
	{
		(void)s; // [TODO]: create rendering info from s
		VkRenderingAttachmentInfo color_attachment = {0};
		VkImageView sc_view;
		ASSERT(
			vk_swapchain_get_current_image_view(&state->swapchain, &sc_view));

		VkClearValue color = {.color = {.float32 = {1.f, 0.f, 1.f, 1.f}}};
		{
			color_attachment.sType =
				VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			color_attachment.imageView = sc_view;
			color_attachment.clearValue = color;
			color_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
			color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		}
		VkRect2D rect = {.extent = state->swapchain.ci.imageExtent,
						 .offset = (VkOffset2D){.x = 0, .y = 0}};
		{
			rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			rendering_info.renderArea = rect;
			rendering_info.colorAttachmentCount = 1;
			rendering_info.pColorAttachments = &color_attachment;
			rendering_info.layerCount = 1;
		}
	}
	vkCmdBeginRendering(cmd_buf, &rendering_info);
}

void vulkan_submit(UNUSED struct gpu_api* self, gpu_command_buffer cmd) {
	vk_api_state* state = self->state;
	vk_cmdbuf_pool_handle vk_handle = {.raw_handle.handle = cmd.handle};
	VkCommandBuffer cmd_buf = vk_command_buffer_get(vk_handle)->command_buffer;

	// TODO: dedicated method:
	vkCmdEndRendering(cmd_buf);

	{ // [TODO]: Hardcode barrier ATTACHMENT_OPTIMAL -> PRESENT_SRC
		VkImage img;
		vk_swapchain_get_current_image(&state->swapchain, &img);
		VkImageMemoryBarrier2 barrier = {0};
		{
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			barrier.image = img;
			barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.srcStageMask =
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstStageMask =
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
									VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.subresourceRange = vk_base_subresource_range();
			barrier.srcQueueFamilyIndex = state->queue_family_idx;
			barrier.dstQueueFamilyIndex = state->queue_family_idx;
		}

		VkDependencyInfo dep_info = {0};
		{
			dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			dep_info.imageMemoryBarrierCount = 1;
			dep_info.pImageMemoryBarriers = &barrier;
		}
		vkCmdPipelineBarrier2(cmd_buf, &dep_info);
	}

	VkSemaphore sc_sem;
	ASSERT(vk_swapchain_get_current_semaphore(&state->swapchain, &sc_sem));

	vk_render_sync frame_sync =
		state->sync_buffer
			.frames[state->frame_idx % self->config.frames_in_flight];

	{ // Submit:
		LOG_WARN("vulkan_render: submitting");
		check_result(vkEndCommandBuffer(cmd_buf));
		VkCommandBufferSubmitInfo cmd_info = {0};
		{
			cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
			cmd_info.commandBuffer = cmd_buf;
			cmd_info.deviceMask = 0;
		}

		VkSemaphoreSubmitInfo signal_sem_info = {0};
		{
			signal_sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			signal_sem_info.pNext = NULL;
			signal_sem_info.semaphore = sc_sem;
			signal_sem_info.stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			signal_sem_info.deviceIndex = 0;
			signal_sem_info.value = 1;
		}

		VkSemaphoreSubmitInfo wait_sem_info = {0};
		{
			wait_sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			wait_sem_info.pNext = NULL;
			wait_sem_info.semaphore = frame_sync.draw_sem;
			wait_sem_info.stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			wait_sem_info.deviceIndex = 0;
			wait_sem_info.value = 1;
		}

		VkSubmitInfo2 submit_info = {0};
		{
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
			submit_info.commandBufferInfoCount = 1;
			submit_info.pCommandBufferInfos = &cmd_info;
			submit_info.signalSemaphoreInfoCount = 1;
			submit_info.pSignalSemaphoreInfos = &signal_sem_info;
			submit_info.waitSemaphoreInfoCount = 1;
			submit_info.pWaitSemaphoreInfos = &wait_sem_info;
		}
		check_result(vkQueueSubmit2(state->queue, 1, &submit_info,
									frame_sync.drawn_fence));
	}
}

void vulkan_end_frame(gpu_api* self) {
	vk_api_state* state = self->state;

	LOG_WARN("vulkan_end_frame: presenting");
	ASSERT(vk_swapchain_present(&state->swapchain, state->queue));

	state->frame_idx++;
	state->frame_in_progress = false;
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
			check_result(vkDeviceWaitIdle(state->device));
			vk_swapchain_destroy(&state->swapchain, state->device);
			vk_surface_destroy(&state->surface, state->instance);
			vmaDestroyAllocator(state->allocator);
			vk_sync_buffer_destroy(state->device, &state->sync_buffer);
			vkDestroyCommandPool(state->device, state->command_pool, NULL);
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

	memset(out, 0, sizeof(*out));
	{
		out->type = GRAPHICS_API_VULKAN;
		out->is_init = false;
		out->name = config.app_name;
		out->config = config;
		out->initialize = vulkan_initialize;
		out->begin_frame = vulkan_begin_frame;
		out->cmd_begin_rendering = vulkan_cmd_begin_rendering;
		out->cmd_submit = vulkan_submit;
		out->end_frame = vulkan_end_frame;
		out->destroy = vulkan_destroy;
	}

	return true;
}

#endif // !GRAPHICS_VULKAN_H
