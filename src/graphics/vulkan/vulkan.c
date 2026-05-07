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

	{ // [TODO]: Sampler
		VkSamplerCreateInfo ci = {0};
		{
			ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			ci.magFilter = VK_FILTER_LINEAR;
			ci.minFilter = VK_FILTER_LINEAR;
			ci.maxLod = VK_LOD_CLAMP_NONE;
		}

		if (!check_result(vkCreateSampler(state->device, &ci, NULL,
										  &state->default_sampler))) {
			LOG_ERR("vulkan_initialize: couldn't create sampler");
			return;
		}
	}

	{ // Descriptor pool
		VkDescriptorPoolSize pool_size = {0};
		{
			pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			pool_size.descriptorCount = 16;
		}

		VkDescriptorPoolCreateInfo ci = {0};
		{
			ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			ci.maxSets = 16;
			ci.poolSizeCount = 1;
			ci.pPoolSizes = &pool_size;
		}

		if (!check_result(vkCreateDescriptorPool(state->device, &ci, NULL,
												 &state->descriptor_pool))) {
			LOG_ERR("vulkan_initialize: couldn't create descriptor pool");
			return;
		}
	}

	{ // Descriptor layout & set
		VkDescriptorSetLayoutBinding bind = {0};
		{
			bind.descriptorCount = 1;
			bind.binding = 0;
			bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bind.pImmutableSamplers = &state->default_sampler;
			bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		VkDescriptorSetLayoutCreateInfo layout_ci = {0};
		{
			layout_ci.sType =
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layout_ci.bindingCount = 1;
			layout_ci.pBindings = &bind;
		}
		if (!check_result(vkCreateDescriptorSetLayout(
				state->device, &layout_ci, NULL, &state->descriptor_layout))) {
			LOG_ERR("vulkan_initialize: couldn't create descriptor set layout");
			return;
		}

		VkDescriptorSetAllocateInfo alloc_ci = {0};
		{
			alloc_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			alloc_ci.descriptorPool = state->descriptor_pool;
			alloc_ci.descriptorSetCount = 1;
			alloc_ci.pSetLayouts = &state->descriptor_layout;
		}

		if (!check_result(vkAllocateDescriptorSets(state->device, &alloc_ci,
												   &state->descriptor))) {
			LOG_ERR("vulkan_initialize: couldn't allocate descriptor");
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
			vkDestroySampler(state->device, state->default_sampler, NULL);
			vkDestroyDescriptorSetLayout(state->device,
										 state->descriptor_layout, NULL);
			vkDestroyDescriptorPool(state->device, state->descriptor_pool,
									NULL);
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
		if ((img_data->usage_flags & required) != required) {
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

gpu_command_buffer vulkan_create_command_buffer(struct gpu_api* self) {
	vk_api_state* state = self->state;
	gpu_command_buffer ret = {.handle = POOL_INVALID_HANDLE};

	VkCommandBufferAllocateInfo alloc_info = {0};
	{
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandBufferCount = 1;
		alloc_info.commandPool = state->command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	}

	VkCommandBuffer cmd_buf;
	if (!check_result(
			vkAllocateCommandBuffers(state->device, &alloc_info, &cmd_buf))) {
		LOG_ERR("vulkan_create_command_buffer: couldn't create command buffer");
		return ret;
	}

	VkCommandBufferBeginInfo begin_info = {0};
	{
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}
	if (!check_result(vkBeginCommandBuffer(cmd_buf, &begin_info))) {
		LOG_ERR("vulkan_create_command_buffer: couldn't begin command buffer");
		return ret;
	}

	vk_cmdbuf_data data = {0};
	{
		data.command_buffer = cmd_buf;
		data.flags = CMDBUF_FLAG_USER;
		data.state = CMDBUF_RECORDING;
	}

	vk_cmdbuf_pool_handle h = vk_cmdbuf_pool_new(state->cmd_pool, &data);
	ret.handle = vk_cmdbuf_pool_unwrap(h);
	return ret;
}

void vulkan_destroy_command_buffer(gpu_api* self, gpu_command_buffer cmd) {
	vk_api_state* state = self->state;

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	if (!cmd_data) {
		LOG_ERR("vulkan_destroy_command_buffer: invalid handle");
		return;
	}

	if (cmd_data->state != CMDBUF_ENDED) {
		LOG_ERR("vulkan_destroy_command_buffer: can't destroy pending command "
				"buffer");
		return;
	}

	if ((cmd_data->flags & CMDBUF_FLAG_FRAME)) {
		LOG_ERR("vulkan_destroy_command_buffer: can't destroy frame command "
				"buffer");
		return;
	}

	vk_cmdbuf_pool_release(state->cmd_pool, to_vk_cmdbuf(cmd));
}

bool vulkan_submit_command_buffer(struct gpu_api* self,
								  gpu_command_buffer cmd) {
	vk_api_state* state = self->state;

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	if (!cmd_data || cmd_data->state != CMDBUF_RECORDING ||
		(cmd_data->flags & CMDBUF_FLAG_FRAME)) {
		LOG_ERR("vulkan_submit_command_buffer: invalid command buffer");
		return false;
	}
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	if (!check_result(vkEndCommandBuffer(cmd_buf))) {
		LOG_ERR("vulkan_submit_command_buffer: couldn't end command buffer");
		return false;
	}
	cmd_data->state = CMDBUF_ENDED;
	VkCommandBufferSubmitInfo cmd_info = {0};
	{
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cmd_info.commandBuffer = cmd_buf;
		cmd_info.deviceMask = 0;
	}

	VkFence fence;
	VkFenceCreateInfo fence_ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};

	if (!check_result(vkCreateFence(state->device, &fence_ci, NULL, &fence))) {
		LOG_ERR("vulkan_submit_command_buffer: couldn't create fence");
		return false;
	}

	VkSubmitInfo2 submit_info = {0};
	{
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		submit_info.commandBufferInfoCount = 1;
		submit_info.pCommandBufferInfos = &cmd_info;
	}
	if (!check_result(vkQueueSubmit2(state->queue, 1, &submit_info, fence))) {
		LOG_ERR("vulkan_submit_command_buffer: couldn't submit");
		return false;
	}

	static uint64_t const fence_timeout = sec_to_ns(1);
	if (!check_result(vkWaitForFences(state->device, 1, &fence, VK_TRUE,
									  fence_timeout))) {
		LOG_ERR("vulkan_submit_command_buffer: couldn't wait for fence");
		return false;
	}

	vkDestroyFence(state->device, fence, NULL);
	return true;
}

bool vulkan_submit_command_buffer_fence(struct gpu_api* self,
										gpu_command_buffer cmd,
										gpu_fence* out_fence) {
	vk_api_state* state = self->state;

	if (!out_fence || !out_fence->internal) {
		LOG_ERR("vulkan_submit_command_buffer_fence: invalid fence");
		return false;
	}

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	if (!cmd_data || cmd_data->state != CMDBUF_RECORDING ||
		!(cmd_data->flags & CMDBUF_FLAG_FRAME)) {
		LOG_ERR("vulkan_submit_command_buffer_fence: invalid command buffer");
		return false;
	}
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;
	VkFence fence = out_fence->internal;

	if (!check_result(vkEndCommandBuffer(cmd_buf))) {
		LOG_ERR("vulkan_submit_command_buffer: couldn't end command buffer");
		return false;
	}
	cmd_data->state = CMDBUF_ENDED;
	VkCommandBufferSubmitInfo cmd_info = {0};
	{
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cmd_info.commandBuffer = cmd_buf;
		cmd_info.deviceMask = 0;
	}

	VkSubmitInfo2 submit_info = {0};
	{
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		submit_info.commandBufferInfoCount = 1;
		submit_info.pCommandBufferInfos = &cmd_info;
	}
	if (!check_result(vkQueueSubmit2(state->queue, 1, &submit_info, fence))) {
		LOG_ERR("vulkan_submit_command_buffer_fence: couldn't submit");
		return false;
	}

	return true;
}

gpu_fence vulkan_create_fence(struct gpu_api* self, bool create_signalled) {
	vk_api_state* state = self->state;
	gpu_fence ret = {0};
	VkFence fence;

	VkFenceCreateInfo fence_ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = create_signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0,
	};

	if (!check_result(vkCreateFence(state->device, &fence_ci, NULL, &fence))) {
		LOG_ERR("vulkan_temp_copy_to_image: couldn't create fence");
		return ret;
	}

	ret.internal = fence;
	return ret;
}

bool vulkan_wait_fence(struct gpu_api* self, gpu_fence gpu_fence,
					   uint64_t timeout_ns) {
	vk_api_state* state = self->state;
	VkFence fence = gpu_fence.internal;

	if (!fence) {
		LOG_ERR("vulkan_wait_fence: invalid fence");
		return false;
	}

	if (!check_result(
			vkWaitForFences(state->device, 1, &fence, VK_TRUE, timeout_ns))) {
		LOG_ERR("vulkan_wait_fence: couldn't wait for fence");
		return false;
	}

	return true;
}

void vulkan_destroy_fence(struct gpu_api* self, gpu_fence gpu_fence) {
	vk_api_state* state = self->state;
	VkFence fence = gpu_fence.internal;
	if (!fence) {
		LOG_ERR("vulkan_destroy_fence: invalid fence");
		return;
	}
	vkDestroyFence(state->device, fence, NULL);
}

bool vulkan_end_frame(gpu_api* self, gpu_frame* f) {
	vk_api_state* state = self->state;
	vk_cmdbuf_pool_handle handle = vk_get_current_sync(state)->command_buffer;
	vk_cmdbuf_data* cmd_data = vk_cmdbuf_pool_get(state->cmd_pool, handle);

	if (f->frame_index != state->frame_idx) {
		LOG_ERR("vulkan_end_frame: invalid frame index");
		return false;
	}

	// Transition framebuffer layout
	if (!vk_transition_to_present(state, f)) {
		LOG_ERR("vulkan_end_frame: couldn't transition swapchain image");
		return false;
	}

	// Submit command buffer
	if (!vk_submit_frame_cmd(state, f)) {
		LOG_ERR("vulkan_end_frame: couldn't submit frame's command buffer");
		return false;
	}

	// Present
	LOG_DEBUG("vulkan_end_frame: presenting");
	if (!vk_swapchain_present(&state->swapchain, state->queue)) {
		LOG_ERR("vulkan_end_frame: couldn't present to swapchain");
		return false;
	}

	// Reset frame's command buffer
	// [TODO]: Should I reset it? Validation errors if I do
	// vkResetCommandBuffer(cmd_data->command_buffer, 0);
	cmd_data->state = CMDBUF_NOT_BEGAN;

	// Advance frame count
	state->frame_idx++;
	state->frame_in_progress = false;
	return true;
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
	vkCmdBindDescriptorSets(cmd_data->command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS, data->layout, 0, 1,
							&state->descriptor, 0, 0);
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

bool vulkan_get_image_full_view(struct gpu_api* self, gpu_image_view* out,
								gpu_image handle) {
	vk_api_state* state = self->state;

	if (!self || !self->is_init) {
		LOG_ERR("vulkan_create_image: invalid parameters");
		return false;
	}

	vk_image_data* vk_img =
		vk_image_pool_get(state->image_pool, to_vk_img(handle));
	if (!vk_img) {
		LOG_ERR("vulkan_get_image_full_view: invalid image handle");
		return false;
	}

	if (vk_img->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN) {
		// User obtained it via begin_frame, so it can only be the current image
		VkImageView* current_swapchain_view;
		ASSERT(vk_image_view_array_at(state->swapchain.image_views,
									  &current_swapchain_view,
									  state->swapchain.img_index));
		out->internal = current_swapchain_view;
	}

	out->internal = vk_img->full_view;
	return true;
}

gpu_sampler vulkan_create_sampler(struct gpu_api* self, gpu_sampler_ci* ci) {
	vk_api_state* state = self->state;
	gpu_sampler ret = {0};

	VkSampler s;
	VkSamplerCreateInfo vk_ci = {0};
	{
		vk_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		vk_ci.addressModeU = gpu_to_vk_address_mode(ci->address_mode_u);
		vk_ci.addressModeV = gpu_to_vk_address_mode(ci->address_mode_v);
		vk_ci.addressModeW = gpu_to_vk_address_mode(ci->address_mode_w);
		vk_ci.mipmapMode = gpu_to_vk_mipmap_mode(ci->mipmap_mode);
		vk_ci.minFilter = gpu_to_vk_filter(ci->min_filter);
		vk_ci.magFilter = gpu_to_vk_filter(ci->mag_filter);
		vk_ci.maxLod = VK_LOD_CLAMP_NONE;
	}

	if (!check_result(vkCreateSampler(state->device, &vk_ci, NULL, &s))) {
		LOG_ERR("vulkan_create_sampler: couldn't create sampler");
		return ret;
	}

	ret.internal = s;

	return ret;
}

void vulkan_destroy_sampler(struct gpu_api* self, gpu_sampler sampler_handle) {
	vk_api_state* state = self->state;
	VkSampler s = sampler_handle.internal;

	if (!s) {
		LOG_ERR("vulkan_destroy_sampler: invalid sampler handle");
		return;
	}

	vkDestroySampler(state->device, s, NULL);
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
		layout_ci.setLayoutCount = 1;
		layout_ci.pSetLayouts = &state->descriptor_layout;
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
		vk_usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (flags & GPU_IMAGE_USAGE_DEPTH_TARGET) {
		vk_usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	if (flags & GPU_IMAGE_USAGE_SAMPLED) {
		vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		vk_usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}

	vk_image_pool_handle vk_img_handle = {0};
	if (is_2d_image(ci->type)) {
		vk_img_handle = vk_create_2d_image(
			state, gpu_to_vk_image_extent(ci->dimensions),
			gpu_to_vk_image_format(ci->format), ci->type, vk_usage_flags,
			ci->mip_levels, ci->array_layers, state->queue_family_idx);

		if (!vk_image_pool_is_handle_valid(state->image_pool, vk_img_handle)) {
			LOG_ERR("vulkan_create_image: couldn't create image");
			return ret;
		}
	} else {
		LOG_ERR("vulkan_create_image: non 2D images aren't supported yet");
		return ret;
	}

	LOG_DEBUG("vulkan_create_image: image created: %u",
			  vk_img_handle.raw_handle.handle);
	ret.handle = vk_image_pool_unwrap(vk_img_handle);

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
	vmaDestroyImage(state->allocator, data->image, data->alloc);
	vk_image_pool_release(state->image_pool, to_vk_img(handle));
}

bool vulkan_create_image_view(struct gpu_api* self, gpu_image_view* out,
							  gpu_image_view_ci* ci) {
	vk_api_state* state = (vk_api_state*)self;

	if (!self || !self->is_init || !ci) {
		LOG_ERR("vulkan_create_image_view: invalid parameters");
		return false;
	}

	vk_image_data* vk_img =
		vk_image_pool_get(state->image_pool, to_vk_img(ci->image));
	if (!vk_img) {
		LOG_ERR("vulkan_create_image_view: invalid image");
		return false;
	}

	if (ci->base_layer + ci->layer_count >= vk_img->layer_count) {
		LOG_ERR("vulkan_create_image_view: invalid layers");
		return false;
	}

	if (ci->base_level + ci->level_count >= vk_img->level_count) {
		LOG_ERR("vulkan_create_image_view: invalid levels");
		return false;
	}

	VkImageSubresourceRange range = vk_custom_subresource_range(
		vk_img->usage_flags, ci->layer_count, ci->level_count, ci->base_level,
		ci->base_layer);

	VkImageView view = {0};
	VkImageViewCreateInfo view_ci = {0};
	{
		view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_ci.image = vk_img->image;
		view_ci.viewType = gpu_to_vk_image_type(vk_img->gpu_type);
		view_ci.subresourceRange = range;
	}

	if (!check_result(
			vkCreateImageView(state->device, &view_ci, NULL, &view))) {
		LOG_ERR("vulkan_create_image_view: couldn't create vulkan image view");
		return false;
	}

	out->internal = view;
	return true;
}

void vulkan_destroy_image_view(struct gpu_api* self, gpu_image_view view) {
	vk_api_state* state = (vk_api_state*)self;

	if (!self || !self->is_init) {
		LOG_ERR("vulkan_create_image_view: invalid parameters");
		return;
	}

	if (!view.internal) {
		LOG_ERR("vulkan_destroy_image_view: invalid view handle");
		return;
	}

	vkDestroyImageView(state->device, view.internal, NULL);
}

bool vulkan_temp_create_image(struct gpu_api* self,
							  UNUSED gpu_command_buffer cmd, gpu_image handle,
							  image_data pixels) {
	vk_api_state* state = self->state;

	VkCommandBufferAllocateInfo alloc_info = {0};
	{
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandBufferCount = 1;
		alloc_info.commandPool = state->command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	}
	VkCommandBuffer cmd_buf;
	if (!check_result(
			vkAllocateCommandBuffers(state->device, &alloc_info, &cmd_buf))) {
		LOG_ERR("vulkan_temp_create_image: couldn't create command buffer");
		return false;
	}

	VkCommandBufferBeginInfo begin_info = {0};
	{
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}
	if (!check_result(vkBeginCommandBuffer(cmd_buf, &begin_info))) {
		LOG_ERR("vulkan_temp_create_image: couldn't begin command buffer");
		return false;
	}

	vk_image_data* data =
		vk_image_pool_get(state->image_pool, to_vk_img(handle));
	if (!data) {
		LOG_ERR("vulkan_temp_create_image: invalid image handle");
		return false;
	}
	if (data->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN) {
		LOG_ERR("vulkan_temp_copy_to_image: can't copy to swapchain image");
		return false;
	}

	VkBufferCreateInfo buf_ci = {0};
	{
		buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		buf_ci.pQueueFamilyIndices = &state->queue_family_idx;
		buf_ci.queueFamilyIndexCount = 1;
		buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buf_ci.size = u8_array_size(pixels.bytes);
	}

	VmaAllocationCreateInfo vma_ci = {0};
	{
		vma_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
					   VMA_ALLOCATION_CREATE_MAPPED_BIT;
		vma_ci.usage = VMA_MEMORY_USAGE_AUTO;
	}

	VmaAllocation alloc;
	VmaAllocationInfo info;
	VkBuffer buffer;

	if (!check_result(vmaCreateBuffer(state->allocator, &buf_ci, &vma_ci,
									  &buffer, &alloc, &info))) {
		LOG_ERR("vulkan_temp_copy_to_image: couldn't create staging buffer");
		return false;
	};

	ASSERT(info.pMappedData != NULL);
	memcpy(info.pMappedData, u8_array_begin(pixels.bytes),
		   u8_array_size(pixels.bytes));

	vk_barrier_info from = gpu_state_to_vk_barrier[GPU_IMAGE_STATE_NONE];
	vk_barrier_info to = gpu_state_to_vk_barrier[GPU_IMAGE_STATE_TRANSFER_DST];
	vk_image_barrier(cmd_buf, data->image, from, to);

	VkImageSubresourceLayers img_layer = {0};
	{
		img_layer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		img_layer.layerCount = data->layer_count;
	}
	VkBufferImageCopy2 img_copy_info = {0};
	{
		VkExtent2D ext2d = data->extent;
		img_copy_info.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
		img_copy_info.imageExtent = (VkExtent3D){
			.width = ext2d.width, .height = ext2d.height, .depth = 1};
		img_copy_info.imageSubresource = img_layer;
	}

	VkCopyBufferToImageInfo2 copy_ci = {0};
	{
		copy_ci.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
		copy_ci.dstImage = data->image;
		copy_ci.dstImageLayout = to.layout;
		copy_ci.pRegions = &img_copy_info;
		copy_ci.regionCount = 1;
		copy_ci.srcBuffer = buffer;
	}

	vkCmdCopyBufferToImage2(cmd_buf, &copy_ci);
	if (!check_result(vkEndCommandBuffer(cmd_buf))) {
		LOG_ERR("vulkan_temp_copy_to_image: couldn't end command buffer");
		return false;
	}

	VkCommandBufferSubmitInfo cmd_info = {0};
	{
		cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cmd_info.commandBuffer = cmd_buf;
		cmd_info.deviceMask = 0;
	}

	VkFence fence;
	VkFenceCreateInfo fence_ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};

	if (!check_result(vkCreateFence(state->device, &fence_ci, NULL, &fence))) {
		LOG_ERR("vulkan_temp_copy_to_image: couldn't create fence");
		return false;
	}

	VkSubmitInfo2 submit_info = {0};
	{
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		submit_info.commandBufferInfoCount = 1;
		submit_info.pCommandBufferInfos = &cmd_info;
	}
	if (!check_result(vkQueueSubmit2(state->queue, 1, &submit_info, fence))) {
		LOG_ERR("vulkan_temp_copy_to_image: couldn't submit");
		return false;
	}

	static uint64_t const fence_timeout = sec_to_ns(1);
	if (!check_result(vkWaitForFences(state->device, 1, &fence, VK_TRUE,
									  fence_timeout))) {
		LOG_ERR("vulkan_temp_copy_to_image: couldn't wait for fence");
		return false;
	}

	vkDestroyFence(state->device, fence, NULL);
	vmaDestroyBuffer(state->allocator, buffer, alloc);
	return true;
}

void vulkan_temp_set_image_desc(gpu_api* self, gpu_image_view gpu_view) {
	vk_api_state* state = self->state;

	VkImageView view = gpu_view.internal;
	VkDescriptorImageInfo img_info = {0};
	{
		img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img_info.imageView = view;
	}
	VkWriteDescriptorSet w = {0};
	{
		w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w.descriptorCount = 1;
		w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		w.dstSet = state->descriptor;
		w.dstBinding = 0;
		w.pImageInfo = &img_info;
	}

	vkUpdateDescriptorSets(state->device, 1, &w, 0, NULL);
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
		out->temp_copy_to_image = vulkan_temp_create_image;
		out->temp_set_image_desc = vulkan_temp_set_image_desc;
		out->create_pipeline = vulkan_create_pipeline;
		out->destroy_pipeline = vulkan_destroy_pipeline;
		out->get_image_extent = vulkan_get_image_extent;
		out->get_image_format = vulkan_get_image_format;
		out->get_image_full_view = vulkan_get_image_full_view;
		out->create_sampler = vulkan_create_sampler;
		out->destroy_sampler = vulkan_destroy_sampler;
		out->create_command_buffer = vulkan_create_command_buffer;
		out->submit_command_buffer = vulkan_submit_command_buffer;
		out->submit_command_buffer_fence = vulkan_submit_command_buffer_fence;
		out->destroy_command_buffer = vulkan_destroy_command_buffer;
		out->create_fence = vulkan_create_fence;
		out->wait_fence = vulkan_wait_fence;
		out->destroy_fence = vulkan_destroy_fence;
		out->create_semaphore = NULL;
		out->destroy_semaphore = NULL;
	}

	return true;
}

gpu_pipeline gpu_pipeline_build(gpu_api* gpu, gpu_pipeline_builder* builder) {
	return vulkan_create_pipeline(gpu, &builder->ci);
}

#endif // !GRAPHICS_VULKAN_H
