#ifndef GRAPHICS_VULKAN_H
#define GRAPHICS_VULKAN_H

#include <vulkan/vulkan.h>

#include "../graphics_internal.h"
#include "vulkan_init_info.h"
#include "vulkan_internal.h"

#include <containers/array.h>
#include <core/core.h>
#include <filesystem/filesystem.h>
#include <graphics/graphics.h>
#include <strings/path.h>

#include <string.h>
#include <vma/vk_mem_alloc.h>

ARRAY_DEFINE_TYPE(VkPhysicalDevice, vk_physical_device_array)
ARRAY_DEFINE_TYPE(VkQueueFamilyProperties2, vk_qf_props_array)
ARRAY_DEFINE_TYPE(VkExtensionProperties, vk_ext_props_array)

#define IMAGE_POOL_SIZE 1024
#define CMDBUF_POOL_SIZE 1024
#define SHADER_POOL_SIZE 1024
#define PIPELINE_POOL_SIZE 512

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

	state->arena = arena;

	// Init resource pools:
	state->image_pool = vk_image_pool_create(arena, IMAGE_POOL_SIZE);
	state->image_view_pool = vk_image_view_pool_create(arena, IMAGE_POOL_SIZE);
	state->cmd_pool = vk_cmdbuf_pool_create(arena, CMDBUF_POOL_SIZE);
	state->shader_pool = vk_shader_pool_create(arena, SHADER_POOL_SIZE);
	state->pipeline_pool = vk_pipeline_pool_create(arena, PIPELINE_POOL_SIZE);

	ASSERT(state->image_pool.raw && state->cmd_pool.raw &&
		   state->shader_pool.raw);

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
		if (!vk_sync_buffer_create(
				state->device, &state->sync_buffer, state->cmd_pool,
				self->config.frames_in_flight, state->command_pool)) {
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

void vulkan_wait_idle(gpu_api* self) {
	vk_api_state* state = self->state;
	check_result(vkDeviceWaitIdle(state->device));
}

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
		vk_cmdbuf_pool_get(state->cmd_pool, frame.frame_sync->command_buffer);
	ASSERT(cmd_data->state == CMDBUF_NOT_BEGAN);
	if (!check_result(
			vkBeginCommandBuffer(cmd_data->command_buffer, &begin_info))) {
		return ret;
	}
	cmd_data->state = CMDBUF_RECORDING;

	ret.frame = vk_to_gpu_frame(&frame);
	ret.ok = true;
	return ret;
}

void vulkan_cmd_begin_rendering(struct gpu_api* self, gpu_command_buffer cmd,
								gpu_render_state* s) {
	vk_api_state* state = self->state;
	vk_cmdbuf_pool_handle vk_handle = to_vk_cmdbuf(cmd);
	vk_cmdbuf_data* cmd_data = vk_cmdbuf_pool_get(state->cmd_pool, vk_handle);
	ASSERT(cmd_data->state == CMDBUF_RECORDING);
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	vk_image_data* img_data = vk_image_pool_get(
		state->image_pool,
		(vk_image_pool_handle){s->color_targets[0].target.handle});

	VkRenderingInfo rendering_info = {0};
	{
		VkRenderingAttachmentInfo color_attachment = {0};

		VkClearValue color = {.color = {.float32 = {1.f, 0.f, 1.f, 1.f}}};
		{
			color_attachment.sType =
				VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			color_attachment.imageView = img_data->full_view;
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
	cmd_data->state = CMDBUF_RENDERING;
}

void vulkan_cmd_end_rendering(struct gpu_api* self, gpu_command_buffer cmd) {
	vk_api_state* state = self->state;
	vk_cmdbuf_pool_handle vk_handle = to_vk_cmdbuf(cmd);
	vk_cmdbuf_data* cmd_data = vk_cmdbuf_pool_get(state->cmd_pool, vk_handle);
	ASSERT(cmd_data->state == CMDBUF_RENDERING);

	VkCommandBuffer cmd_buf = cmd_data->command_buffer;
	vkCmdEndRendering(cmd_buf);
	cmd_data->state = CMDBUF_RECORDING;
}

bool vulkan_cmd_transition_image(gpu_api* self, gpu_command_buffer cmd_handle,
								 gpu_image img_handle,
								 gpu_image_state new_state) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd_handle));
	vk_image_data* img_data =
		vk_image_pool_get(state->image_pool, to_vk_img(img_handle));

	vk_cmdbuf_state cmd_state = cmd_data->state;
	if (cmd_state != CMDBUF_RECORDING && cmd_state != CMDBUF_RENDERING) {
		LOG_ERR("vk_transition_image: invalid command buffer state");
		return false;
	}

	gpu_image_state cur_state = img_data->gpu_state;
	if (new_state == cur_state) {
		LOG_DEBUG("vk_transition_image: transition to same usage");
		return true;
	}

	if (new_state == GPU_IMAGE_STATE_NONE) {
		LOG_ERR("vk_transition_image: invalid transition");
		return false;
	}

	if (img_data->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN) {
		// [TODO]: maybe other invariants?
		switch (new_state) {
		case GPU_IMAGE_STATE_DEPTH_READ:
		case GPU_IMAGE_STATE_DEPTH_WRITE:
			LOG_ERR(
				"vk_transition_image: invalid transition for swapchain image");
			return false;
		default: break;
		}
	} else {
		// Check that image supports new layout
		VkImageUsageFlags required = gpu_state_to_vk_state_flags[new_state];
		if ((img_data->vk_image_flags & required) != required) {
			breakpoint();
			LOG_ERR("vk_transition_image: transition to unsupported usage");
			return false;
		}
	}

	vk_image_barrier(cmd_data->command_buffer, img_data->image,
					 gpu_state_to_vk_barrier[img_data->gpu_state],
					 gpu_state_to_vk_barrier[new_state]);

	// [TODO]: sync nightmare i won't think about it for now
	img_data->gpu_state = new_state;

	return true;
}

bool vulkan_cmd_transition_for_present(struct gpu_api* self,
									   gpu_command_buffer cmd_handle,
									   gpu_image img_handle) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd_handle));
	vk_image_data* img_data =
		vk_image_pool_get(state->image_pool, to_vk_img(img_handle));

	vk_cmdbuf_state cmd_state = cmd_data->state;
	if (cmd_state != CMDBUF_RECORDING && cmd_state != CMDBUF_RENDERING) {
		LOG_ERR("vk_cmd_transition_for_present: invalid command buffer state");
		return false;
	}

	if (!(img_data->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN)) {
		LOG_ERR("vk_cmd_transition_for_present: image isn't a swapchain image");
		return false;
	}

	vk_barrier_info from = gpu_state_to_vk_barrier[img_data->gpu_state];
	static vk_barrier_info const to = {
		.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.access = VK_ACCESS_2_NONE,
	};
	vk_image_barrier(cmd_data->command_buffer, img_data->image, from, to);

	// Reset usage for next frame
	img_data->gpu_state = GPU_IMAGE_STATE_NONE;

	return true;
}

void vulkan_submit(struct gpu_api* self, gpu_command_buffer cmd) {
	vk_api_state* state = self->state;

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));
	ASSERT(cmd_data->state == CMDBUF_RECORDING);
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	VkSemaphore sc_sem;
	ASSERT(vk_swapchain_get_current_semaphore(&state->swapchain, &sc_sem));

	vk_render_sync frame_sync =
		state->sync_buffer
			.frames[state->frame_idx % self->config.frames_in_flight];

	{ // Submit:
		LOG_DEBUG("vulkan_render: submitting");
		check_result(vkEndCommandBuffer(cmd_buf));
		cmd_data->state = CMDBUF_ENDED;
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

void vulkan_end_frame(gpu_api* self, gpu_frame* f) {
	vk_api_state* state = self->state;
	vk_cmdbuf_pool_handle handle = vk_get_current_sync(state)->command_buffer;

	vk_cmdbuf_data* cmd_data = vk_cmdbuf_pool_get(state->cmd_pool, handle);

	// cmd_submit must have been called before ending frame
	ASSERT(cmd_data->state == CMDBUF_ENDED);

	if (f->frame_index != state->frame_idx) {
		LOG_ERR("vulkan_end_frame: invalid frame index");
		return;
	}

#ifdef _DEBUG
	{
		// Swapchain image must be PRESENT_SRC with cmd_transition_for_present
		vk_image_data* sc_img =
			vk_image_pool_get(state->image_pool, to_vk_img(f->backbuffer));
		ASSERT(sc_img->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN);
		ASSERT(sc_img->gpu_state == GPU_IMAGE_STATE_NONE);
	}
#endif

	LOG_DEBUG("vulkan_end_frame: presenting");
	ASSERT(vk_swapchain_present(&state->swapchain, state->queue));

	// Reset frame's command buffer
	// [TODO]: Should I reset it? Validation errors if I do
	// vkResetCommandBuffer(cmd_data->command_buffer, 0);
	cmd_data->state = CMDBUF_NOT_BEGAN;

	// Advance frame count
	state->frame_idx++;
	state->frame_in_progress = false;
}

void vulkan_cmd_set_viewport(struct gpu_api* self, gpu_command_buffer cmd,
							 gpu_image_extent extent) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RECORDING);

	VkViewport vp = {.width = extent.width,
					 .height = extent.height,
					 .minDepth = 0.f,
					 .maxDepth = 1.f};
	vkCmdSetViewport(cmd_data->command_buffer, 0, 1, &vp);
}

void vulkan_cmd_set_scissor(struct gpu_api* self, gpu_command_buffer cmd,
							gpu_scissor scissor) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RECORDING);
	VkRect2D vk_sc = {
		.extent = gpu_to_vk_image_extent(scissor.extent),
		.offset = (VkOffset2D){.x = scissor.offset.x, .y = scissor.offset.y}};
	vkCmdSetScissor(cmd_data->command_buffer, 0, 1, &vk_sc);
}

void vulkan_cmd_reset_viewport(struct gpu_api* self, gpu_command_buffer cmd) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RECORDING);

	VkExtent2D extent = state->swapchain.ci.imageExtent;
	VkViewport vp = {.width = extent.width,
					 .height = extent.height,
					 .minDepth = 0.f,
					 .maxDepth = 1.f};
	vkCmdSetViewport(cmd_data->command_buffer, 0, 1, &vp);
}

void vulkan_cmd_reset_scissor(struct gpu_api* self, gpu_command_buffer cmd) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RECORDING);
	VkRect2D s = {.extent = state->swapchain.ci.imageExtent,
				  .offset = (VkOffset2D){.x = 0, .y = 0}};
	vkCmdSetScissor(cmd_data->command_buffer, 0, 1, &s);
}

void vulkan_cmd_bind_pipeline(struct gpu_api* self, gpu_command_buffer cmd,
							  gpu_pipeline pipeline) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RECORDING ||
		   cmd_data->state == CMDBUF_RENDERING);

	vk_pipeline_data* data =
		vk_pipeline_pool_get(state->pipeline_pool, to_vk_pipeline(pipeline));
	ASSERT(data && data->pipeline);
	vkCmdBindPipeline(cmd_data->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					  data->pipeline);
}

void vulkan_cmd_draw(struct gpu_api* self, gpu_command_buffer cmd,
					 uint32_t vertex_count, uint32_t instance_count,
					 uint32_t first_vertex, uint32_t first_instance) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RENDERING);

	vkCmdDraw(cmd_data->command_buffer, vertex_count, instance_count,
			  first_vertex, first_instance);
}

gpu_image_extent vulkan_get_image_extent(struct gpu_api* self,
										 gpu_image gpu_img) {
	vk_api_state* state = self->state;
	vk_image_data* data =
		vk_image_pool_get(state->image_pool, to_vk_img(gpu_img));
	ASSERT(data);
	return vk_to_gpu_image_extent(data->extent);
}

gpu_image_format vulkan_get_image_format(struct gpu_api* self,
										 gpu_image gpu_img) {
	vk_api_state* state = self->state;
	vk_image_data* data =
		vk_image_pool_get(state->image_pool, to_vk_img(gpu_img));
	ASSERT(data);
	return vk_to_gpu_image_format(data->format);
}

gpu_image_view vulkan_get_image_full_view(struct gpu_api* self,
										  gpu_image handle) {
	vk_api_state* state = self->state;
	gpu_image_view ret = {.handle = POOL_INVALID_HANDLE};

	if (!self || !self->is_init) {
		LOG_ERR("vulkan_create_image: invalid parameters");
		return ret;
	}

	vk_image_data* vk_img =
		vk_image_pool_get(state->image_pool, to_vk_img(handle));
	if (!vk_img) {
		LOG_ERR("vulkan_get_image_full_view: invalid image handle");
		return ret;
	}

	if (vk_img->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN) {
		// Swapchain image doesn't have its view in the cache
		LOG_ERR("vulkan_get_image_full_view: swapchain image isn't supported "
				"yet :/");
		return ret;
	}

	// View isn't in pool
	if (!vk_image_view_pool_is_handle_valid(
			state->image_view_pool,
			to_vk_image_view(vk_img->full_view_handle))) {
		LOG_ERR("vulkan_get_image_full_view: image doesn't have full view");
		return ret;
	}

	ret = vk_img->full_view_handle;
	return ret;
}

gpu_image_view_info_result
vulkan_get_image_view_info(struct gpu_api* self, gpu_image_view view_handle) {
	vk_api_state* state = self->state;
	gpu_image_view_info_result ret = {0};

	if (!self || !self->is_init) {
		LOG_ERR("vulkan_get_image_view_info: invalid parameters");
		return ret;
	}

	vk_image_view* data = vk_image_view_pool_get(state->image_view_pool,
												 to_vk_image_view(view_handle));

	if (!data) {
		LOG_ERR("vulkan_get_image_view_info: invalid view handle");
		return ret;
	}

	{
		ret.layer_count = data->layer_count;
		ret.level_count = data->level_count;
		ret.base_layer = data->base_layer;
		ret.base_level = data->base_level;
		ret.image.handle = vk_image_pool_unwrap(data->image);
		ret.ok = true;
	}
	return ret;
}

gpu_shader vulkan_create_shader(struct gpu_api* self, gpu_shader_ci* ci) {

	gpu_shader ret = {.handle = POOL_INVALID_IDX};
	if (!self || !self->state || !self->is_init || !ci) {
		LOG_ERR("vulkan_create_shader: invalid parameters");
		return ret;
	}

	vk_api_state* state = self->state;

	uint64_t code_size = u8_array_size(ci->code);
	if (code_size == 0 || code_size % 4 != 0) {
		LOG_ERR("vulkan_create_shader: invalid shader code buffer");
		return ret;
	}

	uint8_t* bytes = u8_array_begin(ci->code);
	ASSERT(((uintptr_t)bytes & 3) == 0); // alignment

	VkShaderModule module = {0};
	{
		VkShaderModuleCreateInfo vk_ci = {0};
		{
			vk_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			vk_ci.pCode = (uint32_t*)bytes;
			vk_ci.codeSize = code_size;
		};

		if (!check_result(
				vkCreateShaderModule(state->device, &vk_ci, NULL, &module))) {
			LOG_ERR("vulkan_create_shader: couldn't create shader");
			return ret;
		}
		// [TODO]: debug name
	}

	vk_shader_data data = {0};
	{
		data.module = module;
		data.stage = gpu_to_vk_shader_stage(ci->type);
		data.name = ci->debug_name;
	}
	vk_shader_pool_handle handle =
		vk_shader_pool_new(state->shader_pool, &data);
	ASSERT(vk_shader_pool_is_handle_valid(state->shader_pool, handle));

	ret.handle = vk_shader_pool_unwrap(handle);
	return ret;
}

void vulkan_destroy_shader(struct gpu_api* self, gpu_shader handle) {
	vk_api_state* state = self->state;
	vk_shader_data* data =
		vk_shader_pool_get(state->shader_pool, to_vk_shader(handle));

	if (!data || !data->module) {
		LOG_ERR("vulkan_destroy_shader: invalid shader handle");
		return;
	}

	// [NOTE]: Shader module and pool handle lifecycle are ALWAYS coupled
	vkDestroyShaderModule(state->device, data->module, NULL);
	vk_shader_pool_release(state->shader_pool, to_vk_shader(handle));
}

gpu_pipeline vulkan_create_pipeline(struct gpu_api* self, gpu_pipeline_ci* ci) {
	vk_api_state* state = self->state;

	gpu_pipeline ret = {.handle = POOL_INVALID_HANDLE};
	vk_pipeline_data data = {0};

	// [TODO]: layout config
	VkPipelineLayoutCreateInfo layout_ci = {0};
	{
		layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_ci.setLayoutCount = 0;
		layout_ci.pushConstantRangeCount = 0;
	}
	if (!check_result(vkCreatePipelineLayout(state->device, &layout_ci, NULL,
											 &data.layout))) {
		LOG_ERR("vulkan_create_pipeline: couldn't create pipeline layout");
		return ret;
	}
	data.ci.layout = data.layout;

	{
		vk_shader_data* vs = vk_shader_pool_get(
			state->shader_pool, to_vk_shader(ci->vertex_shader));
		vk_shader_data* fs = vk_shader_pool_get(
			state->shader_pool, to_vk_shader(ci->fragment_shader));

		if (!vs || !vs->module || !fs || !fs->module) {
			LOG_ERR("gpu_to_vulkan_pipeline: invalid shader handles");
			return ret;
		}
		ASSERT(vs->stage == VK_SHADER_STAGE_VERTEX_BIT &&
			   fs->stage == VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipelineViewportStateCreateInfo viewport_ci = {0};
		{
			viewport_ci.sType =
				VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewport_ci.pNext = NULL;
			viewport_ci.viewportCount = 1;
			viewport_ci.scissorCount = 1;
		}
		VkPipelineColorBlendAttachmentState blend_state = {
			.colorWriteMask = 0xF,
		};
		VkPipelineColorBlendStateCreateInfo blend_ci = {0};
		{
			blend_ci.sType =
				VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blend_ci.pNext = NULL;
			blend_ci.attachmentCount = 1;
			blend_ci.pAttachments = &blend_state;
		}
		VkPipelineShaderStageCreateInfo shader_stages[2] = {
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			 .stage = VK_SHADER_STAGE_VERTEX_BIT,
			 .module = vs->module,
			 .pName = "main"},
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			 .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			 .module = fs->module,
			 .pName = "main"}};

		// No need for vertex input:
		VkPipelineVertexInputStateCreateInfo vert_input_ci = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

		VkFormat formats[GPU_MAX_RENDERTARGETS] = {0};
		for (uint32_t i = 0; i < ci->color_target_count; ++i) {
			formats[i] = gpu_to_vk_image_format(ci->color_targets[i].format);
		}
		VkPipelineRenderingCreateInfo _renderInfo = {0};
		{
			_renderInfo.sType =
				VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
			_renderInfo.colorAttachmentCount = ci->color_target_count;
			_renderInfo.pColorAttachmentFormats = formats;
			_renderInfo.depthAttachmentFormat =
				gpu_to_vk_image_format(ci->depth.depth_target_format);
		};

		VkPipelineRasterizationStateCreateInfo raster_ci = {0};
		{
			raster_ci.sType =
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			raster_ci.cullMode = gpu_to_vk_cull_mode(ci->rasterizer.cull_mode);
			raster_ci.polygonMode =
				gpu_to_vk_polygon_mode(ci->rasterizer.polygon_mode);
			raster_ci.frontFace =
				gpu_to_vk_winding(ci->rasterizer.winding_order);
			raster_ci.lineWidth = 1.f;
		}
		VkPipelineMultisampleStateCreateInfo multisampling_ci = {0};
		{
			multisampling_ci.sType =
				VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		};
		VkPipelineDepthStencilStateCreateInfo depth_ci = {0};
		{
			depth_ci.sType =
				VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depth_ci.depthTestEnable = ci->depth.depth_test;
			depth_ci.depthWriteEnable = ci->depth.depth_write;
			depth_ci.stencilTestEnable = VK_FALSE;
			depth_ci.depthCompareOp =
				gpu_to_vk_compare_op(ci->depth.depth_compare_op);
		}
		VkPipelineInputAssemblyStateCreateInfo input_asm = {0};
		{
			input_asm.sType =
				VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			input_asm.topology =
				gpu_to_vk_primitive_type(ci->rasterizer.primitive_type);
		}

		VkDynamicState dyn_state[] = {VK_DYNAMIC_STATE_VIEWPORT,
									  VK_DYNAMIC_STATE_SCISSOR};

		VkPipelineDynamicStateCreateInfo dynamic_ci = {0};
		{
			dynamic_ci.sType =
				VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamic_ci.pDynamicStates = &dyn_state[0];
			dynamic_ci.dynamicStateCount = 2;
		};

		{
			data.ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			data.ci.pNext = &_renderInfo;
			data.ci.stageCount = 2;
			data.ci.pStages = shader_stages;
			data.ci.pVertexInputState = &vert_input_ci;
			data.ci.pInputAssemblyState = &input_asm;
			data.ci.pViewportState = &viewport_ci;
			data.ci.pRasterizationState = &raster_ci;
			data.ci.pMultisampleState = &multisampling_ci;
			data.ci.pColorBlendState = &blend_ci;
			data.ci.pDepthStencilState = &depth_ci;
			data.ci.layout = data.layout;
			data.ci.pDynamicState = &dynamic_ci;
		}
	}

	if (!check_result(vkCreateGraphicsPipelines(
			state->device, NULL, 1, &data.ci, NULL, &data.pipeline))) {
		LOG_ERR("vulkan_create_pipeline: couldn't create pipeline");
		vkDestroyPipelineLayout(state->device, data.layout, NULL);
		return ret;
	}

	vk_pipeline_pool_handle h =
		vk_pipeline_pool_new(state->pipeline_pool, &data);
	ret.handle = vk_pipeline_pool_unwrap(h);

	return ret;
}

void vulkan_destroy_pipeline(struct gpu_api* self, gpu_pipeline handle) {
	vk_api_state* state = self->state;
	vk_pipeline_data* data =
		vk_pipeline_pool_get(state->pipeline_pool, to_vk_pipeline(handle));

	if (!data || !data->pipeline) {
		LOG_ERR("vulkan_destroy_pipeline: invalid pipeline handle");
		return;
	}

	// [NOTE]: Pipeline and pool handle lifecycle are ALWAYS coupled
	vkDestroyPipelineLayout(state->device, data->layout, NULL);
	vkDestroyPipeline(state->device, data->pipeline, NULL);
	vk_pipeline_pool_release(state->pipeline_pool, to_vk_pipeline(handle));
}

gpu_image vulkan_create_image(gpu_api* self, gpu_image_ci* ci) {
	vk_api_state* state = self->state;
	gpu_image ret = {.handle = POOL_INVALID_HANDLE};

	if (!self || !self->is_init || !ci) {
		LOG_ERR("vulkan_create_image: invalid parameters");
		return ret;
	}

	if (!gpu_validate_image_ci(ci)) {
		LOG_ERR("vulkan_create_image: invalid create info");
		return ret;
	}

	gpu_image_usage flags = ci->usage;
	VkImageUsageFlags vk_usage_flags = 0;

	if (flags & GPU_IMAGE_USAGE_RENDER_TARGET) {
		vk_usage_flags &= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (flags & GPU_IMAGE_USAGE_DEPTH_TARGET) {
		vk_usage_flags &= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	if (flags & GPU_IMAGE_USAGE_SAMPLED) {
		vk_usage_flags &= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		vk_usage_flags &= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	vk_image_pool_handle vk_img_handle = {0};
	if (ci->type == GPU_IMAGE_TYPE_2D || ci->type == GPU_IMAGE_TYPE_2D_ARRAY) {
		vk_img_handle = vk_create_2d_image(
			state, gpu_to_vk_image_extent(ci->dimensions),
			gpu_to_vk_image_format(ci->format), vk_usage_flags, ci->mip_levels,
			ci->array_layers, state->queue_family_idx);

		if (!vk_image_pool_is_handle_valid(state->image_pool, vk_img_handle)) {
			LOG_ERR("vulkan_create_image: couldn't create image");
			return ret;
		}
	} else {
		// [TODO]: handle cubes
		LOG_ERR("vulkan_create_image: cube images aren't supported yet");
		return ret;
	}

	LOG_DEBUG("vulkan_create_image: image created: %u",
			  vk_img_handle.raw_handle.handle);
	ret.handle = vk_image_pool_unwrap(vk_img_handle);

	// Add full view to pool:
	vk_image_view_pool_handle full_view_handle =
		vk_create_full_image_view_handle(state, vk_img_handle);
	if (!vk_image_view_pool_is_handle_valid(state->image_view_pool,
											full_view_handle)) {
		LOG_ERR("vulkan_create_image: couldn't allocate image's full view");
		return ret;
	}


	return ret;
}

void vulkan_destroy_image(struct gpu_api* self, gpu_image handle) {
	vk_api_state* state = self->state;

	vk_image_data* data =
		vk_image_pool_get(state->image_pool, to_vk_img(handle));
	if (!data) {
		LOG_ERR("vulkan_destroy_image: invalid image handle");
		return;
	}

	if (data->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN) {
		LOG_ERR("vulkan_destroy_image: can't destroy swapchain image");
		return;
	}

	vkDestroyImageView(state->device, data->full_view, NULL);
	vk_image_view_pool_release(state->image_view_pool,
							   to_vk_image_view(data->full_view_handle));

	vmaDestroyImage(state->allocator, data->image, data->alloc);
	vk_image_pool_release(state->image_pool, to_vk_img(handle));

	// [TODO]: auto-remove these once they are stored in a proper container
	if (data->user_view_count != 0) {
		LOG_WARN("vulkan_destroy_image: %u views haven't been destroyed",
				 data->user_view_count);
	}
}

gpu_image_view vulkan_create_image_view(struct gpu_api* self,
										gpu_image_view_ci* ci) {
	gpu_image_view ret = {.handle = POOL_INVALID_HANDLE};
	vk_api_state* state = (vk_api_state*)self;

	if (!self || !self->is_init || !ci) {
		LOG_ERR("vulkan_create_image_view: invalid parameters");
		return ret;
	}

	vk_image_data* vk_img =
		vk_image_pool_get(state->image_pool, to_vk_img(ci->image));
	if (!vk_img) {
		LOG_ERR("vulkan_create_image_view: invalid image");
		return ret;
	}

	if (ci->base_layer + ci->layer_count >= vk_img->layer_count) {
		LOG_ERR("vulkan_create_image_view: invalid layers");
		return ret;
	}

	if (ci->base_level + ci->level_count >= vk_img->level_count) {
		LOG_ERR("vulkan_create_image_view: invalid levels");
		return ret;
	}

	vk_image_view_pool_handle view_handle = VK_IMGVIEWPOOL_INVALID;
	bool cached = false;
	// [TODO]: cache lookup
	if (!cached) {
		// Create the view:
		VkImageSubresourceRange range = vk_custom_subresource_range(
			vk_img->usage_flags, ci->layer_count, ci->level_count,
			ci->base_level, ci->base_layer);

		VkImageView view = {0};
		VkImageViewCreateInfo view_ci = {0};
		{
			view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view_ci.image = vk_img->image;
			view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D; // [TODO]: array&cube
			view_ci.subresourceRange = range;
		}

		if (!check_result(
				vkCreateImageView(state->device, &view_ci, NULL, &view))) {
			LOG_ERR(
				"vulkan_create_image_view: couldn't create vulkan image view");
			return ret;
		}

		// Cache it:
		vk_image_view view_data = {0};
		{
			view_data.image = to_vk_img(ci->image);
			view_data.view = view;
			view_data.base_layer = ci->base_layer;
			view_data.base_level = ci->base_level;
			view_data.layer_count = ci->layer_count;
			view_data.layer_count = ci->level_count;
		}

		view_handle =
			vk_image_view_pool_new(state->image_view_pool, &view_data);
		if (!vk_image_view_pool_is_handle_valid(state->image_view_pool,
												view_handle)) {
			LOG_ERR("vulkan_create_image_view: couldn't cache image view");
			vkDestroyImageView(state->device, view, NULL);
			return ret;
		}
	}

	ret.handle = vk_image_view_pool_unwrap(view_handle);
	return ret;
}

void vulkan_destroy_image_view(struct gpu_api* self, gpu_image_view handle) {
	vk_api_state* state = (vk_api_state*)self;

	if (!self || !self->is_init) {
		LOG_ERR("vulkan_create_image_view: invalid parameters");
		return;
	}

	vk_image_view* data = vk_image_view_pool_get(state->image_view_pool,
												 to_vk_image_view(handle));

	if (!data) {
		LOG_ERR("vulkan_destroy_image_view: invalid view handle");
		return;
	}

	vkDestroyImageView(state->device, data->view, NULL);
	vk_image_view_pool_release(state->image_view_pool,
							   to_vk_image_view(handle));
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
		out->destroy = vulkan_destroy;
		out->wait_idle = vulkan_wait_idle;
		out->begin_frame = vulkan_begin_frame;
		out->cmd_begin_rendering = vulkan_cmd_begin_rendering;
		out->cmd_end_rendering = vulkan_cmd_end_rendering;
		out->cmd_transition_image = vulkan_cmd_transition_image;
		out->cmd_transition_for_present = vulkan_cmd_transition_for_present;
		out->cmd_submit = vulkan_submit;
		out->cmd_set_viewport = vulkan_cmd_set_viewport;
		out->cmd_set_scissor = vulkan_cmd_set_scissor;
		out->cmd_reset_viewport = vulkan_cmd_reset_viewport;
		out->cmd_reset_scissor = vulkan_cmd_reset_scissor;
		out->cmd_bind_pipeline = vulkan_cmd_bind_pipeline;
		out->cmd_draw = vulkan_cmd_draw;
		out->end_frame = vulkan_end_frame;
		out->create_shader = vulkan_create_shader;
		out->destroy_shader = vulkan_destroy_shader;
		out->create_image = vulkan_create_image;
		out->destroy_image = vulkan_destroy_image;
		out->create_image_view = vulkan_create_image_view;
		out->destroy_image_view = vulkan_destroy_image_view;
		out->create_pipeline = vulkan_create_pipeline;
		out->destroy_pipeline = vulkan_destroy_pipeline;
		out->get_image_extent = vulkan_get_image_extent;
		out->get_image_format = vulkan_get_image_format;
		out->get_image_full_view = vulkan_get_image_full_view;
		out->get_image_view_info = vulkan_get_image_view_info;
	}

	return true;
}

gpu_pipeline gpu_pipeline_build(gpu_api* gpu, gpu_pipeline_builder* builder) {
	return vulkan_create_pipeline(gpu, &builder->ci);
}

#endif // !GRAPHICS_VULKAN_H
