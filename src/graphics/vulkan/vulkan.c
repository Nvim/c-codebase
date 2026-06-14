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

#define IMAGE_POOL_SIZE 1024U
#define IMAGE_VIEW_POOL_SIZE 2048U
#define CMDBUF_POOL_SIZE 1024U
#define SHADER_POOL_SIZE 512U
#define PIPELINE_POOL_SIZE 1024U
#define BUFFER_POOL_SIZE 4096U
#define HANDLE_POOL_SIZE 1024U

static VkFormat const _default_surface_format = VK_FORMAT_B8G8R8A8_UNORM;

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
	VkPhysicalDeviceDescriptorIndexingProperties descriptor_indexing_props;
	VkPhysicalDeviceMaintenance4Properties maintenance4_props;
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
		if (_is_dgpu(device_props)) { break; }
		// Keep searching in case we get dgpu later
	}

	if (i == device_count && !ret.found) {
		LOG_ERR("vulkan_initialize: no suitable GPU found");
		memset(&ret, 0, sizeof(ret));
		return ret;
	}

	// Get props again in case we fallback on earlier device.

	// Also get descriptor indexing props and max buffer size
	ret.descriptor_indexing_props.sType =
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT;

	// Also get max buffer size
	ret.maintenance4_props.sType =
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES;
	ret.maintenance4_props.pNext = &ret.descriptor_indexing_props;

	ret.device_props.pNext = &ret.maintenance4_props;
	vkGetPhysicalDeviceProperties2(ret.device, &ret.device_props);
	return ret;
}

static VkDevice _create_device(VkPhysicalDevice physical_device,
							   uint32_t queue_idx) {

	VkPhysicalDeviceVulkan11Features vk_11_features = {0};
	{
		vk_11_features.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		vk_11_features.shaderDrawParameters = VK_TRUE;
	}

	VkPhysicalDeviceVulkan12Features vk_12_features = {0};
	{
		vk_12_features.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vk_12_features.pNext = &vk_11_features;
		vk_12_features.descriptorIndexing = true;
		vk_12_features.shaderSampledImageArrayNonUniformIndexing = true;
		vk_12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		vk_12_features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
		vk_12_features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
		// vk_12_features.descriptorBindingPartiallyBound = VK_TRUE;
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

	VkPhysicalDeviceFeatures vk_10_features = {.samplerAnisotropy = VK_TRUE,
											   .shaderInt64 = VK_TRUE,
											   .multiDrawIndirect = VK_TRUE};

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

	VkPhysicalDeviceFeatures2 features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	vkGetPhysicalDeviceFeatures2(physical_device, &features);

	VkDevice device = NULL;
	check_result(vkCreateDevice(physical_device, &device_ci, NULL, &device));
	return device;
}

// [TODO]: move this to graphics.c
static bool validate_gpu_config(gpu_api_config* config) {
	if (!config->frames_in_flight ||
		config->frames_in_flight > GPU_MAX_BUFFERING) {
		LOG_ERR("validate_gpu_config: invalid buffering amount");
		return false;
	}
	return true;
}

typedef struct _memory_lookup_result {
	uint64_t shared;
	uint64_t device_total;
} _memory_lookup_result;

static _memory_lookup_result
_memory_lookup(UNUSED arena_t* scratch,
			   VkPhysicalDeviceMemoryProperties* props) {
	_memory_lookup_result ret = {0};

	u8_array shared_seen = u8_array_create(scratch, props->memoryHeapCount);
	u8_array device_seen = u8_array_create(scratch, props->memoryHeapCount);
	if (!shared_seen.raw || !device_seen.raw) { return ret; }
	u8_array_clear(shared_seen);
	u8_array_clear(device_seen);

	LOG_DEBUG("looking up %u memory heaps (%u types)", props->memoryHeapCount,
			  props->memoryTypeCount);
	for (uint32_t i = 0; i < props->memoryTypeCount; ++i) {
		VkMemoryType mem = props->memoryTypes[i];
		VkMemoryPropertyFlags flags = mem.propertyFlags;

		uint8_t* skip_shared;
		uint8_t* skip_device;
		ASSERT(u8_array_at(shared_seen, &skip_shared, mem.heapIndex));
		ASSERT(u8_array_at(device_seen, &skip_device, mem.heapIndex));

		VkMemoryHeap heap = props->memoryHeaps[mem.heapIndex];

		bool device_local = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		bool host_visible = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		bool host_coherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		bool host_cached = (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		/**
		 * [NOTE]: We label memory as shared only if it is host coherent.
		 * Technically we could use non-coherent, but it would require specific
		 * logic everywhere for flushing. I ain't doin that
		 */
		if (device_local && host_visible && host_coherent) {
			if (!*skip_shared) { ret.shared += heap.size; }
			*skip_shared = true;
		}

		if (device_local) {
			if (!*skip_device) { ret.device_total += heap.size; }
			*skip_device = true;
		}

		// Logging:
		if (device_local && host_visible) {
			LOG_DEBUG("[#%u](DEVICE_LOCAL|HOST_VISIBLE): " STR_FMT " - coherent: %u, cached: %u", mem.heapIndex, STR_FMT_ARGS(fs_format_memory_size(scratch, heap.size)), host_coherent, host_cached); // clang-format-ignore
		} else if (device_local) {
			LOG_DEBUG("[#%u](DEVICE_LOCAL): " STR_FMT, mem.heapIndex,
					  STR_FMT_ARGS(fs_format_memory_size(scratch, heap.size)));
		} else {
			LOG_DEBUG("[#%u](HOST_LOCAL): " STR_FMT " - coherent: %u, cached: %u", mem.heapIndex, STR_FMT_ARGS(fs_format_memory_size(scratch, heap.size)), host_coherent, host_cached);  // clang-format-ignore
		}
	}

	return ret;
}

void vulkan_initialize(gpu_api* self, window_t* window, arena_t* arena,
					   arena_t* scratch) {
	if (!self || !arena || !scratch || !window) {
		LOG_ERR("vulkan_initialize: invalid parameters");
		return;
	}

	if (!validate_gpu_config(&self->config)) { return; }

	self->window = window;
	uint64_t arena_base_usage = arena_get_usage(arena);
	vk_api_state* state = arena_push_z(arena, vk_api_state);
	if (!state) {
		LOG_ERR("vulkan_initialize: couldn't allocate state");
		ASSERT(false);
		return;
	}

	state->arena = arena;

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

	gpu_api_config* cfg = &self->config;

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

		// Set max limits according to props
		VkPhysicalDeviceDescriptorIndexingProperties p =
			dev_result.descriptor_indexing_props;
		state->descriptor_props = p;

		cfg->max_sampled_images =
			((p.maxPerStageDescriptorUpdateAfterBindSampledImages <
			  cfg->max_sampled_images)
				 ? p.maxPerStageDescriptorUpdateAfterBindSampledImages
				 : cfg->max_sampled_images);

		cfg->max_storage_images =
			MIN(p.maxPerStageDescriptorUpdateAfterBindStorageImages,
				cfg->max_storage_images);

		cfg->max_samplers = MIN(p.maxPerStageDescriptorUpdateAfterBindSamplers,
								cfg->max_samplers);

		{
			// clang-format off
			cfg->max_sampled_images = OR_ELSE(cfg->max_sampled_images, IMAGE_POOL_SIZE);
			cfg->max_storage_images = OR_ELSE(cfg->max_storage_images, IMAGE_POOL_SIZE);
			cfg->max_samplers = OR_ELSE(cfg->max_samplers, HANDLE_POOL_SIZE);
			cfg->max_command_buffers = OR_ELSE(cfg->max_command_buffers, CMDBUF_POOL_SIZE);
			cfg->max_shader_programs = OR_ELSE(cfg->max_shader_programs, SHADER_POOL_SIZE);
			cfg->max_pipelines = OR_ELSE(cfg->max_pipelines, PIPELINE_POOL_SIZE);
			cfg->max_buffers = OR_ELSE(cfg->max_buffers, BUFFER_POOL_SIZE);
			// clang-format on
		}

		state->max_buffer_size = dev_result.maintenance4_props.maxBufferSize;
		state->max_buffer_range =
			state->device_props.properties.limits.maxStorageBufferRange;
		state->push_constant_size =
			state->device_props.properties.limits.maxPushConstantsSize;

		// clang-format off
		LOG_INFO("vulkan_initialize: max sampled images: %u", cfg->max_sampled_images);
		LOG_INFO("vulkan_initialize: max storage images: %u", cfg->max_storage_images);
		LOG_INFO("vulkan_initialize: max samplers: %u", cfg->max_samplers);
		LOG_INFO("vulkan_initialize: max buffer size: " STR_FMT, STR_FMT_ARGS(fs_format_memory_size(scratch, state->max_buffer_size)));
		LOG_INFO("vulkan_initialize: max buffer range: " STR_FMT, STR_FMT_ARGS(fs_format_memory_size(scratch, state->max_buffer_range)));
		LOG_INFO("vulkan_initialize: max push constant size: " STR_FMT, STR_FMT_ARGS(fs_format_memory_size(scratch, state->push_constant_size)));
		// clang-format on

		// Search available memory types for Device Local + Host Visible amount
		state->memory_props.sType =
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
		vkGetPhysicalDeviceMemoryProperties2(state->physical_device,
											 &state->memory_props);

		LOG_INFO("======= MEMORY LOOKUP ========");
		_memory_lookup_result r =
			_memory_lookup(scratch, &state->memory_props.memoryProperties);
		if (!r.device_total) {
			LOG_ERR("vulkan_initialize: couldn't get memory info");
			return;
		}
		state->device_memory = r.device_total;
		state->shared_memory = r.shared;
		state->shared_memory_ratio = r.shared / (double)r.device_total;
		LOG_INFO("SHARED: " STR_FMT ". TOTAL: " STR_FMT " (%.2f %)", STR_FMT_ARGS(fs_format_memory_size(scratch, r.shared)), STR_FMT_ARGS(fs_format_memory_size(scratch, r.device_total)), state->shared_memory_ratio*100); // clang-format-ignore
		LOG_INFO("==============================");
	}

	{ // Init resource pools:
		size_t image_pool_size =
			cfg->max_sampled_images + cfg->max_storage_images;

		state->image_pool = vk_image_pool_create(arena, image_pool_size);

		state->image_view_pool =
			vk_image_view_pool_create(arena, image_pool_size);

		state->cmd_pool =
			vk_cmdbuf_pool_create(arena, cfg->max_command_buffers);

		state->shader_pool =
			vk_shader_pool_create(arena, cfg->max_shader_programs);

		state->pipeline_pool =
			vk_pipeline_pool_create(arena, cfg->max_pipelines);
		state->buffer_pool = vk_buffer_pool_create(arena, cfg->max_buffers);
		state->handle_pool = vk_handle_pool_create(arena, cfg->max_samplers);
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

		check_result(vmaCreateAllocator(&alloc_ci, &state->allocator));
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

		VkPhysicalDeviceSurfaceInfo2KHR surface_info = {0};
		{
			surface_info.sType =
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
			surface_info.surface = state->surface.surface;
		}
		uint32_t surface_formats_count = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(state->physical_device,
											 state->surface.surface,
											 &surface_formats_count, NULL);

		VkSurfaceFormatKHR* surface_formats = arena_push_array_z(
			state->arena, VkSurfaceFormatKHR, surface_formats_count);

		vkGetPhysicalDeviceSurfaceFormatsKHR(
			state->physical_device, state->surface.surface,
			&surface_formats_count, surface_formats);

		bool format_supported = false;
		VkFormat wanted =
			gpu_to_vk_image_format(self->config.target_image_format);
		LOG_DEBUG("vulkan_initialize: looking for surface format %d", wanted);
		for (uint32_t i = 0; i < surface_formats_count; ++i) {
			if (surface_formats[i].format == wanted) {
				format_supported = true;
				state->rendertarget_format = wanted;
				break;
			}
		}

		if (!format_supported) {
			state->rendertarget_format = _default_surface_format;
			self->config.target_image_format =
				vk_to_gpu_image_format(_default_surface_format);
			LOG_DEBUG("vulkan_initialize: fallback to default surface format");
		}
	}

	// Swapchain:
	{
		window_size win_size = platform_get_window_size(window);

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
								 state->rendertarget_format, state->image_pool,
								 state->image_view_pool)) {
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

	{ // Descriptor pool
		VkDescriptorPoolSize tex_pool_size = {0};
		{
			tex_pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			tex_pool_size.descriptorCount = self->config.max_sampled_images;
		}
		VkDescriptorPoolSize storage_pool_size = {0};
		{
			storage_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			storage_pool_size.descriptorCount = self->config.max_storage_images;
		}
		VkDescriptorPoolSize sampler_pool_size = {0};
		{
			sampler_pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
			sampler_pool_size.descriptorCount = self->config.max_samplers;
		}

		VkDescriptorPoolSize pool_sizes[3] = {tex_pool_size, storage_pool_size,
											  sampler_pool_size};

		VkDescriptorPoolCreateInfo ci = {0};
		{
			ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			ci.maxSets = 16;
			ci.poolSizeCount = countof(pool_sizes);
			ci.pPoolSizes = pool_sizes;
			ci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		}

		if (!check_result(vkCreateDescriptorPool(state->device, &ci, NULL,
												 &state->descriptor_pool))) {
			LOG_ERR("vulkan_initialize: couldn't create descriptor pool");
			return;
		}
	}

	{ // Descriptor layout & set
		VkDescriptorSetLayoutBinding texture_binding = {0};
		{
			texture_binding.descriptorCount = self->config.max_sampled_images;
			texture_binding.binding = TEXTURE_BINDING;
			texture_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			texture_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		VkDescriptorSetLayoutBinding storage_binding = {0};
		{
			storage_binding.descriptorCount = self->config.max_storage_images;
			storage_binding.binding = STORAGE_IMAGE_BINDING;
			storage_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			storage_binding.stageFlags =
				VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		VkDescriptorSetLayoutBinding sampler_binding = {0};
		{
			sampler_binding.descriptorCount = self->config.max_samplers;
			sampler_binding.binding = SAMPLER_BINDING;
			sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			sampler_binding.stageFlags =
				VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		VkDescriptorSetLayoutBinding bindings_array[3] = {
			texture_binding, storage_binding, sampler_binding};

		// Create descriptor set layout with indexing feature:
		VkDescriptorBindingFlags binding_flags =
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo layout_binding_flags = {0};
		{
			layout_binding_flags.sType =
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			layout_binding_flags.bindingCount = countof(bindings_array);
			layout_binding_flags.pBindingFlags = &binding_flags;
		};

		VkDescriptorSetLayoutCreateInfo layout_ci = {0};
		{
			layout_ci.sType =
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layout_ci.pNext = &layout_binding_flags;
			layout_ci.bindingCount = countof(bindings_array);
			layout_ci.pBindings = bindings_array;
			layout_ci.flags =
				VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		}
		if (!check_result(vkCreateDescriptorSetLayout(
				state->device, &layout_ci, NULL, &state->descriptor_layout))) {
			LOG_ERR("vulkan_initialize: couldn't create descriptor set layout");
			return;
		}

		// Allocate descriptor arrays for images:
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
	size_t alloc = arena_get_usage(state->arena) - arena_base_usage;
	LOG_INFO("vulkan_initialize: allocated " STR_FMT " for gpu api",
			 STR_FMT_ARGS(fs_format_memory_size(scratch, alloc)));
}

void vulkan_destroy(gpu_api* self) {
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

gpu_memory_info vulkan_get_memory_info(gpu_api* self) {
	ASSERT(self->is_init && self->state);
	vk_api_state* state = self->state;
	return (gpu_memory_info){
		.device_memory = state->device_memory,
		.shared_memory = state->shared_memory,
		.ratio = state->shared_memory_ratio,
	};
}

void vulkan_wait_idle(gpu_api* self) {
	vk_api_state* state = self->state;
	check_result(vkDeviceWaitIdle(state->device));
}

gpu_frame_result vulkan_begin_frame(gpu_api* self) {
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

	if (frame.has_rendertarget == true) {
		ret.status = FRAME_OK;
	} else {
		LOG_INFO("vulkan_begin_frame: frame must be skipped by client");
		ret.status = FRAME_SKIP;
	}

	ret.frame = vk_to_gpu_frame(&frame);
	return ret;
}

void vulkan_cmd_begin_rendering(gpu_api* self, gpu_command_buffer cmd,
								gpu_render_state* s) {
	vk_api_state* state = self->state;
	vk_cmdbuf_pool_handle vk_handle = to_vk_cmdbuf(cmd);
	vk_cmdbuf_data* cmd_data = vk_cmdbuf_pool_get(state->cmd_pool, vk_handle);
	ASSERT(cmd_data->state == CMDBUF_RECORDING);
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	vk_image_view_data* target = vk_image_view_pool_get(
		state->image_view_pool, to_vk_imgview(s->color_targets[0].target));
	if (!target || target->type != GPU_IMAGE_VIEW_RENDER_TARGET) {
		LOG_ERR("vulkan_cmd_begin_rendering: invalid render target view");
		return;
	}

	VkRenderingInfo rendering_info = {0};
	VkClearValue color = {.color = {.float32 = {1.f, 0.f, 1.f, 1.f}}};

	VkRenderingAttachmentInfo color_attachment = {0};
	{
		color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		color_attachment.imageView = target->view;
		color_attachment.clearValue = color;
		color_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		color_attachment.loadOp =
			gpu_to_vk_load_op(s->color_targets[0].load_op);
		color_attachment.storeOp =
			gpu_to_vk_store_op(s->color_targets[0].store_op);
	}

	VkRenderingAttachmentInfo depth_attachment = {0};
	bool has_depth = (s->depth_target != NULL);
	if (has_depth) {
		vk_image_view_data* depth_view = vk_image_view_pool_get(
			state->image_view_pool, to_vk_imgview(s->depth_target->target));
		if (!depth_view || depth_view->type != GPU_IMAGE_VIEW_DEPTH_TARGET) {
			LOG_ERR("vulkan_cmd_begin_rendering: invalid depth target view");
			return;
		}
		VkClearValue clear =
			(VkClearValue){.depthStencil.depth = s->depth_target->clear_value};

		depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_attachment.imageView = depth_view->view;
		depth_attachment.clearValue = clear;
		depth_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		depth_attachment.loadOp = gpu_to_vk_load_op(s->depth_target->load_op);
		depth_attachment.storeOp =
			gpu_to_vk_store_op(s->depth_target->store_op);
	}

	VkRect2D rect = {.extent = state->swapchain.ci.imageExtent,
					 .offset = (VkOffset2D){.x = 0, .y = 0}};
	{
		rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		rendering_info.renderArea = rect;
		rendering_info.colorAttachmentCount = 1;
		rendering_info.pColorAttachments = &color_attachment;
		if (has_depth) rendering_info.pDepthAttachment = &depth_attachment;
		rendering_info.layerCount = 1;
	}
	vkCmdBeginRendering(cmd_buf, &rendering_info);
	cmd_data->state = CMDBUF_RENDERING;
}

void vulkan_cmd_end_rendering(gpu_api* self, gpu_command_buffer cmd) {
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
		VkImageUsageFlags required =
			gpu_image_state_to_vk_state_flags[new_state];
		if ((img_data->usage_flags & required) != required) {
			breakpoint();
			LOG_ERR("vk_transition_image: transition to unsupported usage");
			return false;
		}
	}

	vk_image_barrier(cmd_data->command_buffer, img_data->image,
					 gpu_state_to_vk_image_barrier[img_data->gpu_state],
					 gpu_state_to_vk_image_barrier[new_state]);

	// [TODO]: sync nightmare i won't think about it for now
	img_data->gpu_state = new_state;

	return true;
}

gpu_command_buffer vulkan_create_command_buffer(gpu_api* self) {
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

bool vulkan_submit_command_buffer(gpu_api* self, gpu_command_buffer cmd) {
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

bool vulkan_submit_command_buffer_fence(gpu_api* self, gpu_command_buffer cmd,
										gpu_fence fence_handle) {
	vk_api_state* state = self->state;

	vk_opaque_handle* h =
		vk_handle_pool_get(state->handle_pool, to_vk_handle(fence_handle));

	if (h->type != VK_OBJECT_TYPE_FENCE || !h->handle) {
		LOG_ERR("vulkan_submit_command_buffer_fence: invalid fence handle");
		return false;
	}
	VkFence fence = h->handle;

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	if (!cmd_data || cmd_data->state != CMDBUF_RECORDING ||
		(cmd_data->flags & CMDBUF_FLAG_FRAME)) {
		LOG_ERR("vulkan_submit_command_buffer_fence: invalid command buffer");
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

gpu_fence vulkan_create_fence(gpu_api* self, bool create_signalled) {
	vk_api_state* state = self->state;
	gpu_fence ret = {0};
	VkFence fence;

	VkFenceCreateInfo fence_ci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = create_signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0,
	};

	if (!check_result(vkCreateFence(state->device, &fence_ci, NULL, &fence))) {
		LOG_ERR("vulkan_create_fence: couldn't create fence");
		return ret;
	}

	vk_opaque_handle handle = {
		.handle = fence,
		.type = VK_OBJECT_TYPE_FENCE,
	};
	vk_handle_pool_handle vk_ret =
		vk_handle_pool_new(state->handle_pool, &handle);

	ret.handle = vk_ret.raw_handle;
	return ret;
}

bool vulkan_wait_fence(gpu_api* self, gpu_fence gpu_fence,
					   uint64_t timeout_ns) {
	vk_api_state* state = self->state;
	vk_opaque_handle* h =
		vk_handle_pool_get(state->handle_pool, to_vk_handle(gpu_fence));

	if (h->type != VK_OBJECT_TYPE_FENCE || !h->handle) {
		LOG_ERR("vulkan_wait_fence: invalid fence handle");
		return false;
	}
	VkFence fence = h->handle;

	if (!check_result(
			vkWaitForFences(state->device, 1, &fence, VK_TRUE, timeout_ns))) {
		LOG_ERR("vulkan_wait_fence: couldn't wait for fence");
		return false;
	}

	return true;
}

void vulkan_destroy_fence(gpu_api* self, gpu_fence gpu_fence) {
	vk_api_state* state = self->state;
	vk_opaque_handle* h =
		vk_handle_pool_get(state->handle_pool, to_vk_handle(gpu_fence));

	if (h->type != VK_OBJECT_TYPE_FENCE || !h->handle) {
		LOG_ERR("vulkan_destroy_fence: invalid fence");
		return;
	}
	VkFence fence = h->handle;
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
	if (check_handle(f->framebuffer.handle)) {
		if (!vk_transition_to_present(state, f)) {
			LOG_ERR("vulkan_end_frame: couldn't transition swapchain image");
			return false;
		}
	}

	// Submit command buffer
	if (!vk_submit_frame_cmd(state, f)) {
		LOG_ERR("vulkan_end_frame: couldn't submit frame's command buffer");
		return false;
	}

	// Present
	if (check_handle(f->framebuffer.handle)) {
		vk_present_result pres_res =
			vk_swapchain_present(&state->swapchain, state->queue);

		if (pres_res == PRESENT_ERROR) {
			LOG_ERR("vulkan_end_frame: couldn't present to swapchain");
			return false;
		}

		if (pres_res == PRESENT_OUT_OF_DATE) {
			LOG_INFO(
				"vulkan_end_frame: queueing swapchain refresh for next frame");
			state->swapchain.must_refresh = true;
		}
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

void vulkan_cmd_set_viewport(gpu_api* self, gpu_command_buffer cmd,
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

void vulkan_cmd_set_scissor(gpu_api* self, gpu_command_buffer cmd,
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

void vulkan_cmd_reset_viewport(gpu_api* self, gpu_command_buffer cmd) {
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

void vulkan_cmd_reset_scissor(gpu_api* self, gpu_command_buffer cmd) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RECORDING);
	VkRect2D s = {.extent = state->swapchain.ci.imageExtent,
				  .offset = (VkOffset2D){.x = 0, .y = 0}};
	vkCmdSetScissor(cmd_data->command_buffer, 0, 1, &s);
}

void vulkan_cmd_bind_pipeline(gpu_api* self, gpu_command_buffer cmd,
							  gpu_pipeline pipeline,
							  gpu_per_frame_data* frame_data) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RECORDING ||
		   cmd_data->state == CMDBUF_RENDERING);
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	vk_pipeline_data* data =
		vk_pipeline_pool_get(state->pipeline_pool, to_vk_pipeline(pipeline));
	ASSERT(data && data->pipeline);
	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, data->pipeline);
	vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
							data->layout, 0, 1, &state->descriptor, 0, 0);

	vk_push_constants pc = {.reserved_address = 0,
							.user_data_buffer =
								frame_data->frame_buffer_address};
	VkPushConstantsInfo pc_info = {0};
	{
		pc_info.sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO;
		pc_info.size = sizeof(vk_push_constants);
		pc_info.stageFlags =
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pc_info.layout = data->layout;
		pc_info.pValues = &pc;
	}
	vkCmdPushConstants2(cmd_buf, &pc_info);
}

void vulkan_cmd_draw(gpu_api* self, gpu_command_buffer cmd,
					 uint32_t vertex_count, uint32_t instance_count,
					 uint32_t first_vertex, uint32_t first_instance) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RENDERING);

	vkCmdDraw(cmd_data->command_buffer, vertex_count, instance_count,
			  first_vertex, first_instance);
}

void vulkan_cmd_draw_indexed(gpu_api* self, gpu_command_buffer cmd,
							 uint32_t index_count, uint32_t instance_count,
							 uint32_t first_index, uint32_t first_instance) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RENDERING);

	vkCmdDrawIndexed(cmd_data->command_buffer, index_count, instance_count,
					 first_index, 0, first_instance);
}

void vulkan_cmd_draw_indirect(gpu_api* self, gpu_command_buffer cmd,
							  gpu_buffer indirect_buffer, uint32_t offset,
							  uint32_t draw_count, uint32_t stride) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RENDERING);

	vk_buffer_data* buf_data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(indirect_buffer));

	if (!buf_data || !buf_data->buffer) {
		LOG_ERR("vulkan_cmd_draw_indirect: invalid buffer");
		return;
	}

	vkCmdDrawIndirect(cmd_data->command_buffer, buf_data->buffer, offset,
					  draw_count, stride);
}

void vulkan_cmd_draw_indirect_indexed(gpu_api* self, gpu_command_buffer cmd,
									  gpu_buffer indirect_buffer,
									  uint32_t offset, uint32_t draw_count,
									  uint32_t stride) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));

	ASSERT(cmd_data->state == CMDBUF_RENDERING);

	vk_buffer_data* buf_data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(indirect_buffer));

	if (!buf_data || !buf_data->buffer) {
		LOG_ERR("vulkan_cmd_draw_indirect_indexed: invalid buffer");
		return;
	}

	vkCmdDrawIndexedIndirect(cmd_data->command_buffer, buf_data->buffer, offset,
							 draw_count, stride);
}

gpu_image_extent vulkan_get_image_extent(gpu_api* self, gpu_image gpu_img) {
	vk_api_state* state = self->state;
	vk_image_data* data =
		vk_image_pool_get(state->image_pool, to_vk_img(gpu_img));
	ASSERT(data);
	return vk_to_gpu_image_extent(data->extent);
}

/**
 * [NOTE] Technically wrong, framebuffer doesn't necessarily span all surface.
 * Will do for now.
 */
gpu_image_extent vulkan_get_framebuffer_extent(gpu_api* self) {
	vk_api_state* state = self->state;
	VkExtent2D ext = state->surface.capabilities.currentExtent;

	return (gpu_image_extent){ext.width, ext.height};
}

gpu_image_format vulkan_get_image_format(gpu_api* self, gpu_image gpu_img) {
	vk_api_state* state = self->state;
	vk_image_data* data =
		vk_image_pool_get(state->image_pool, to_vk_img(gpu_img));
	ASSERT(data);
	return vk_to_gpu_image_format(data->format);
}

gpu_sampler vulkan_create_sampler(gpu_api* self, gpu_sampler_ci* ci,
								  uint32_t* out_shader_index) {
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

	vk_opaque_handle handle = {
		.handle = s,
		.type = VK_OBJECT_TYPE_SAMPLER,
	};
	vk_handle_pool_handle vk_ret =
		vk_handle_pool_new(state->handle_pool, &handle);

	uint32_t index = vk_update_sampler_descriptor(state, vk_ret);
	if (index == INVALID_DESCRIPTOR) {
		LOG_ERR("vulkan_create_sampler: couldn't get sampler shader binding");
		return ret;
	}

	ret.handle = vk_ret.raw_handle;
	*out_shader_index = index;
	return ret;
}

void vulkan_destroy_sampler(gpu_api* self, gpu_sampler sampler_handle) {
	vk_api_state* state = self->state;
	vk_opaque_handle* h =
		vk_handle_pool_get(state->handle_pool, to_vk_handle(sampler_handle));

	if (h->type != VK_OBJECT_TYPE_SAMPLER || !h->handle) {
		LOG_ERR("vulkan_destroy_sampler: invalid sampler handle");
		return;
	}

	vkDestroySampler(state->device, h->handle, NULL);
	vk_handle_pool_release(state->handle_pool, to_vk_handle(sampler_handle));
}

gpu_shader vulkan_create_shader(gpu_api* self, gpu_shader_ci* ci) {

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

void vulkan_destroy_shader(gpu_api* self, gpu_shader handle) {
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

gpu_pipeline vulkan_create_pipeline(gpu_api* self, gpu_pipeline_ci* ci) {
	vk_api_state* state = self->state;

	gpu_pipeline ret = {.handle = POOL_INVALID_HANDLE};
	vk_pipeline_data data = {0};

	// [TODO]: layout config
	VkPushConstantRange pc_range = {0};
	{
		pc_range.offset = 0;
		pc_range.size = sizeof(vk_push_constants);
		pc_range.stageFlags =
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkPipelineLayoutCreateInfo layout_ci = {0};
	{
		layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_ci.setLayoutCount = 1;
		layout_ci.pSetLayouts = &state->descriptor_layout;
		layout_ci.pushConstantRangeCount = 1;
		layout_ci.pPushConstantRanges = &pc_range;
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

		if (!check_result(vkCreateGraphicsPipelines(
				state->device, NULL, 1, &data.ci, NULL, &data.pipeline))) {
			LOG_ERR("vulkan_create_pipeline: couldn't create pipeline");
			vkDestroyPipelineLayout(state->device, data.layout, NULL);
			return ret;
		}
	}

	vk_pipeline_pool_handle h =
		vk_pipeline_pool_new(state->pipeline_pool, &data);
	ret.handle = vk_pipeline_pool_unwrap(h);

	return ret;
}

void vulkan_destroy_pipeline(gpu_api* self, gpu_pipeline handle) {
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
	if (flags & GPU_IMAGE_STORAGE_READ) {
		vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		vk_usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
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

void vulkan_destroy_image(gpu_api* self, gpu_image handle) {
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

	vmaDestroyImage(state->allocator, data->image, data->alloc);
	vk_image_pool_release(state->image_pool, to_vk_img(handle));
}

gpu_image_view vulkan_create_image_view(gpu_api* self, gpu_image_view_ci* ci) {
	vk_api_state* state = self->state;

	gpu_image_view out = {.handle = POOL_INVALID_HANDLE};

	if (!self || !self->is_init || !ci) {
		LOG_ERR("vulkan_create_image_view: invalid parameters");
		return out;
	}

	vk_image_data* vk_img =
		vk_image_pool_get(state->image_pool, to_vk_img(ci->image));
	if (!vk_img) {
		LOG_ERR("vulkan_create_image_view: invalid image");
		return out;
	}

	if (ci->base_layer >= ci->layer_count) {
		LOG_ERR("vulkan_create_image_view: invalid layers");
		return out;
	}
	if (ci->base_level >= ci->level_count) {
		LOG_ERR("vulkan_create_image_view: invalid levels");
		return out;
	}

	uint16_t layer_count = ci->layer_count;
	if (layer_count == GPU_COUNT_REMAINING) {
		layer_count = vk_img->layer_count - ci->base_layer;
	}
	uint16_t level_count = ci->level_count;
	if (level_count == GPU_COUNT_REMAINING) {
		level_count = vk_img->level_count - ci->base_level;
	}

	if (ci->type == GPU_IMAGE_VIEW_TEXTURE) {
		if (ci->base_layer + layer_count > vk_img->layer_count) {
			LOG_ERR("vulkan_create_image_view: invalid layers");
			return out;
		}

		if (ci->base_level + level_count > vk_img->level_count) {
			LOG_ERR("vulkan_create_image_view: invalid levels");
			return out;
		}

	} else {
		if (layer_count != 1 || level_count != 1) {
			LOG_ERR(
				"vulkan_create_image_view: layer and level count must be 1");
			return out;
		}
	}

	VkImageViewType view_type = 0;
	VkImageUsageFlags want = 0;

	switch (ci->type) {
	case GPU_IMAGE_VIEW_TEXTURE:
		// Textures views can have any type
		view_type = gpu_to_vk_image_type(vk_img->gpu_type);
		want = VK_IMAGE_USAGE_SAMPLED_BIT;
		break;
	case GPU_IMAGE_VIEW_STORAGE:
		// Storage views can have any type, but are limited to one mip/layer
		view_type = gpu_to_vk_image_type(vk_img->gpu_type);
		want = VK_IMAGE_USAGE_STORAGE_BIT;
		break;
	case GPU_IMAGE_VIEW_RENDER_TARGET:
		// Render targets have to be 2D, or single layers of an arrays
		// Cubes are turned into 6-level arrays
		view_type = vk_img->gpu_type == GPU_IMAGE_TYPE_2D
						? VK_IMAGE_VIEW_TYPE_2D
						: VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		want = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		break;
	case GPU_IMAGE_VIEW_DEPTH_TARGET:
		// Depth images have to be 2D, enforced in gpu_validate_image_ci.
		view_type = VK_IMAGE_VIEW_TYPE_2D;
		want = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		break;
		;
	default: ASSERT(false);
	}

	if (!(vk_img->usage_flags & want)) {
		LOG_ERR(
			"vulkan_create_image_view: image doesn't support this view type");
		return out;
	}

	// We enforced non-textures to have single mip and level already
	VkImageSubresourceRange range = vk_custom_subresource_range(
		vk_img->usage_flags, layer_count, level_count, ci->base_level,
		ci->base_layer);

	VkImageViewCreateInfo view_ci = {0};
	{
		view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_ci.image = vk_img->image;
		view_ci.viewType = view_type;
		view_ci.format = vk_img->format;
		view_ci.subresourceRange = range;
	}

	VkImageView view = {0};
	if (!check_result(
			vkCreateImageView(state->device, &view_ci, NULL, &view))) {
		LOG_ERR("vulkan_create_image_view: couldn't create vulkan image view");
		return out;
	}

	vk_image_view_data data = {0};
	{
		data.image = to_vk_img(ci->image);
		data.type = ci->type;
		data.view = view;
	}
	if (ci->type == GPU_IMAGE_VIEW_TEXTURE) {
		data.data.texture_info.base_layer = ci->base_layer;
		data.data.texture_info.layer_count = layer_count;
		data.data.texture_info.base_level = ci->base_level;
		data.data.texture_info.level_count = level_count;

	} else {
		data.data.view_info.layer = ci->base_layer;
		data.data.view_info.level = ci->base_level;
	}

	vk_image_view_pool_handle ret =
		vk_image_view_pool_new(state->image_view_pool, &data);

	if (has_image_descriptor(ci->type)) {
		LOG_INFO("vulkan_create_image_view: updating image descriptor");
		if (!vk_update_image_descriptor(state, ret)) {
			LOG_ERR("vulkan_create_image_view: couldn't write descriptor");
			return out;
		}
	}

	out.handle = vk_image_view_pool_unwrap(ret);
	return out;
}

bool vulkan_get_image_view_shader_index(struct gpu_api* self,
										gpu_image_view view, uint32_t* out) {
	vk_api_state* state = self->state;
	vk_image_view_data* data =
		vk_image_view_pool_get(state->image_view_pool, to_vk_imgview(view));
	if (!data || !data->view) {
		LOG_ERR("vulkan_get_image_view_shader_index: invalid view handle");
		return false;
	}

	if (data->type != GPU_IMAGE_VIEW_TEXTURE &&
		data->type != GPU_IMAGE_VIEW_STORAGE) {
		LOG_ERR("vulkan_get_image_view_shader_index: invalid view type");
		return false;
	}

	*out = data->descriptor_index;
	return true;
}

void vulkan_destroy_image_view(gpu_api* self, gpu_image_view view) {
	vk_api_state* state = self->state;

	if (!self || !self->is_init) {
		LOG_ERR("vulkan_create_image_view: invalid parameters");
		return;
	}

	vk_image_view_data* data =
		vk_image_view_pool_get(state->image_view_pool, to_vk_imgview(view));
	if (!data || !data->view) {
		LOG_ERR("vulkan_destroy_image_view: invalid view handle");
		return;
	}

	vkDestroyImageView(state->device, data->view, NULL);
	vk_image_view_pool_release(state->image_view_pool, to_vk_imgview(view));
}

bool vulkan_cmd_copy_to_image(struct gpu_api* self, gpu_command_buffer cmd,
							  gpu_image_copy_info* info) {
	vk_api_state* state = self->state;

	if (!info) {
		LOG_ERR("vulkan_cmd_copy_to_image: invalid command buffer state");
		return false;
	}

#define MAX_REGIONS 256U
	size_t region_count = gpu_image_region_array_size(info->regions);
	if (!region_count || region_count > MAX_REGIONS) {
		LOG_ERR("vulkan_cmd_copy_to_image: invalid region count");
		return false;
	}

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));
	if (cmd_data->state != CMDBUF_RECORDING) {
		LOG_ERR("vulkan_cmd_copy_to_image: invalid command buffer state");
		return false;
	};
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	vk_image_data* data =
		vk_image_pool_get(state->image_pool, to_vk_img(info->dst));
	if (!data) {
		LOG_ERR("vulkan_cmd_copy_to_image: invalid image handle");
		return false;
	}
	if (data->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN) {
		LOG_ERR("vulkan_cmd_copy_to_image: can't update to swapchain image");
		return false;
	}

	vk_buffer_data* buf_data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(info->src));
	if (!buf_data) {
		LOG_ERR("vulkan_cmd_copy_to_image: invalid staging buffer");
		return false;
	}

	if (!(buf_data->usage_flags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) ||
		!buf_data->alloc_info.pMappedData) {
		LOG_ERR("vulkan_cmd_copy_to_image: invalid staging buffer usage");
		return false;
	}

	// Barrier to TRANSFER_DST:
	vk_image_barrier_info from = gpu_state_to_vk_image_barrier[data->gpu_state];
	vk_image_barrier_info to =
		gpu_state_to_vk_image_barrier[GPU_IMAGE_STATE_TRANSFER_DST];
	vk_image_barrier(cmd_buf, data->image, from, to);
	data->gpu_state = GPU_IMAGE_STATE_TRANSFER_DST;

	VkImageAspectFlags aspect = 0;
	if (data->usage_flags & (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT; // [TODO]: stencil?
	} else {
		aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkBufferImageCopy2 copies[MAX_REGIONS] = {0};
	size_t offset = info->src_offset;

	gpu_image_region* reg;
	for (uint32_t i = 0; i < region_count; ++i) {
		ASSERT(gpu_image_region_array_at(info->regions, &reg, i));

		ASSERT(reg->layer < data->layer_count &&
			   reg->level < data->level_count);

		uint32_t mip_w = MAX(1u, data->extent.width >> reg->level);
		uint32_t mip_h = MAX(1u, data->extent.height >> reg->level);
		VkExtent3D extent = {
			.width = mip_w,
			.height = mip_h,
			.depth = 1,
		};

		VkImageSubresourceLayers subresource = {
			.aspectMask = aspect,
			.mipLevel = reg->level,
			.baseArrayLayer = reg->layer,
			.layerCount = 1,
		};

		{
			copies[i].sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
			copies[i].imageExtent = extent;
			copies[i].imageSubresource = subresource;
			copies[i].bufferOffset = offset;
		}

		uint32_t bytes_per_pixel = gpu_get_format_bytes_per_pixel(
			vk_to_gpu_image_format(data->format));
		offset += (mip_w * mip_h * bytes_per_pixel);
	}

	VkCopyBufferToImageInfo2 copy_ci = {0};
	{
		copy_ci.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
		copy_ci.dstImage = data->image;
		copy_ci.dstImageLayout = to.layout;
		copy_ci.pRegions = copies;
		copy_ci.regionCount = region_count;
		copy_ci.srcBuffer = buf_data->buffer;
	}

	vkCmdCopyBufferToImage2(cmd_buf, &copy_ci);

#undef MAX_REGIONS
	return true;
}

// Fills out array. Assumed to be an array of 2 VkExtent3D.
static void _get_blit_offsets(VkOffset3D* out, uint32_t width, uint32_t height,
							  gpu_image_region* reg) {
	uint32_t mip_w = MAX(1u, width >> reg->level);
	uint32_t mip_h = MAX(1u, height >> reg->level);
	out[0] = (VkOffset3D){0};
	out[1] = (VkOffset3D){
		.x = mip_w,
		.y = mip_h,
		.z = 1,
	};
}

static void _get_blit_subresource(VkImageSubresourceLayers* out,
								  VkImageUsageFlags flags,
								  gpu_image_region* reg) {
	VkImageAspectFlags aspect = 0;
	if (flags & (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT; // [TODO]: stencil?
	} else {
		aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	*out = (VkImageSubresourceLayers){
		.aspectMask = aspect,
		.mipLevel = reg->level,
		.baseArrayLayer = reg->layer,
		.layerCount = 1,
	};
}

bool vulkan_cmd_blit_to_image(gpu_api* self, gpu_command_buffer cmd,
							  gpu_image_blit_info* info) {

	vk_api_state* state = self->state;

	if (!info) {
		LOG_ERR("vulkan_cmd_blit_to_image: parameters");
		return false;
	}

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));
	if (cmd_data->state != CMDBUF_RECORDING) {
		LOG_ERR("vulkan_cmd_blit_to_image: invalid command buffer state");
		return false;
	};
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	vk_image_data* dst_data =
		vk_image_pool_get(state->image_pool, to_vk_img(info->dst));
	if (!dst_data) {
		LOG_ERR("vulkan_cmd_blit_to_image: invalid dest image handle");
		return false;
	}
	vk_image_data* src_data =
		vk_image_pool_get(state->image_pool, to_vk_img(info->src));
	if (!src_data) {
		LOG_ERR("vulkan_cmd_blit_to_image: invalid src image handle");
		return false;
	}

	// Validation time
	if (dst_data->vk_image_flags & IMAGE_FLAGS_SWAPCHAIN) {
		LOG_ERR("vulkan_cmd_blit_to_image: can't blit to swapchain image");
		return false;
	}

	if (!(src_data->usage_flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
		!(dst_data->usage_flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
		LOG_ERR("vulkan_cmd_blit_to_image: images are missing transfer usage "
				"flags");
		return false;
	}

	if (info->src_region.layer >= src_data->layer_count ||
		info->src_region.level >= src_data->level_count) {
		LOG_ERR("vulkan_cmd_blit_to_image: invalid src layer/level count");
		return false;
	}

	if (info->dst_region.layer >= dst_data->layer_count ||
		info->dst_region.level >= dst_data->level_count) {
		LOG_ERR("vulkan_cmd_blit_to_image: invalid dst layer/level count");
		return false;
	}

	// Transition source & destination:
	vk_image_barrier(
		cmd_buf, src_data->image,
		gpu_state_to_vk_image_barrier[src_data->gpu_state],
		gpu_state_to_vk_image_barrier[GPU_IMAGE_STATE_TRANSFER_SRC]);
	src_data->gpu_state = GPU_IMAGE_STATE_TRANSFER_SRC;

	vk_image_barrier(
		cmd_buf, dst_data->image,
		gpu_state_to_vk_image_barrier[dst_data->gpu_state],
		gpu_state_to_vk_image_barrier[GPU_IMAGE_STATE_TRANSFER_DST]);
	dst_data->gpu_state = GPU_IMAGE_STATE_TRANSFER_DST;

	VkImageBlit2 reg = {0};
	{
		reg.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

		_get_blit_subresource(&reg.srcSubresource, src_data->usage_flags,
							  &info->src_region);
		_get_blit_offsets(reg.srcOffsets, src_data->extent.width,
						  src_data->extent.height, &info->src_region);

		_get_blit_subresource(&reg.dstSubresource, dst_data->usage_flags,
							  &info->dst_region);
		_get_blit_offsets(reg.dstOffsets, dst_data->extent.width,
						  dst_data->extent.height, &info->dst_region);
	}

	VkBlitImageInfo2 blit_info = {0};
	{
		blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;

		blit_info.srcImage = src_data->image;
		blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		blit_info.dstImage = dst_data->image;
		blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		blit_info.filter = gpu_to_vk_filter(info->filter);
		blit_info.pRegions = &reg;
		blit_info.regionCount = 1;
	}

	vkCmdBlitImage2(cmd_buf, &blit_info);
	return true;
}

gpu_buffer vulkan_create_buffer(gpu_api* self, gpu_buffer_ci* ci) {
	gpu_buffer ret = {.handle = POOL_INVALID_HANDLE};
	if (!self || !ci || !self->is_init) {
		LOG_ERR("vulkan_create_buffer: invalid parameters");
		return ret;
	}

	vk_api_state* state = self->state;

	if (!gpu_validate_buffer_ci(ci)) {
		LOG_ERR("vulkan_create_buffer: invalid buffer create info");
		return ret;
	}

	if (!ci->size || ci->size > state->max_buffer_size) {
		LOG_ERR("vulkan_create_buffer: invalid buffer size");
		return ret;
	}

	vk_buffer_data data = {0};
	VmaAllocationCreateInfo vma_ci = gpu_to_vk_buffer_memory(ci);
	VkBufferCreateInfo buf_ci = {0};
	{
		buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_ci.usage = gpu_to_vk_buffer_usage(ci->usage);
		buf_ci.size = ci->size;
		buf_ci.pQueueFamilyIndices = &state->queue_family_idx;
		buf_ci.queueFamilyIndexCount = 1;
		buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buf_ci.flags = 0;
	}

	if (!check_result(vmaCreateBuffer(state->allocator, &buf_ci, &vma_ci,
									  &data.buffer, &data.alloc,
									  &data.alloc_info))) {
		LOG_ERR("vulkan_create_buffer: couldn't create buffer");
		return ret;
	};

	// Likely overkill check, but will remind me to do the mapping here if I
	// every switch to my own allocator
	if (vma_ci.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
		if (!data.alloc_info.pMappedData) {
			LOG_ERR("vulkan_create_buffer: couldn't get buffer mapping");
			vmaDestroyBuffer(state->allocator, data.buffer, data.alloc);
			return ret;
		}
	}

	// Store memory property flags as VMA might not always allocate what user
	// would prefer
	VkMemoryType mem = state->memory_props.memoryProperties
						   .memoryTypes[data.alloc_info.memoryType];
	VkBufferDeviceAddressInfo deviceAdressInfo = {0};
	{
		deviceAdressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		deviceAdressInfo.buffer = data.buffer;
	}

	data.size = buf_ci.size;
	data.memory_flags = mem.propertyFlags;
	data.usage_flags = buf_ci.usage;
	if (data.usage_flags & VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT) {
		data.device_address =
			vkGetBufferDeviceAddress(state->device, &deviceAdressInfo);
	}

	vk_buffer_pool_handle vk_ret =
		vk_buffer_pool_new(state->buffer_pool, &data);
	ret.handle = vk_buffer_pool_unwrap(vk_ret);

	return ret;
}

void vulkan_destroy_buffer(gpu_api* self, gpu_buffer buf) {
	vk_api_state* state = self->state;

	vk_buffer_data* data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(buf));

	if (!data || !data->buffer) {
		LOG_ERR("vulkan_destroy_buffer: invalid buffer");
		return;
	}

	vmaDestroyBuffer(state->allocator, data->buffer, data->alloc);
}

gpu_buffer_map_result vulkan_map_buffer(gpu_api* self, gpu_buffer buf) {
	gpu_buffer_map_result res = {0};
	vk_api_state* state = self->state;

	vk_buffer_data* data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(buf));

	if (!data || !data->buffer) {
		LOG_ERR("vulkan_map_buffer: invalid buffer");
		return res;
	}

	if (!data->alloc_info.pMappedData) {
		LOG_ERR("vulkan_map_buffer: buffer has no mapped data");
		return res;
	}

	res.ptr = data->alloc_info.pMappedData;
	res.ok = true;

	return res;
}

gpu_buffer_device_address_result
vulkan_buffer_get_device_address(gpu_api* self, gpu_buffer buf) {
	gpu_buffer_device_address_result res = {0};
	vk_api_state* state = self->state;

	vk_buffer_data* data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(buf));

	if (!data || !data->buffer) {
		LOG_ERR("vulkan_buffer_get_device_address: invalid buffer");
		return res;
	}

	if (!data->device_address) {
		LOG_ERR(
			"vulkan_buffer_get_device_address: buffer has no device address");
		return res;
	}

	res.address = data->device_address;
	res.ok = true;

	return res;
}

bool vulkan_cmd_bind_index_buffer(gpu_api* self, gpu_command_buffer cmd,
								  gpu_buffer buf_handle) {
	vk_api_state* state = self->state;
	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));
	if (cmd_data->state != CMDBUF_RECORDING &&
		cmd_data->state != CMDBUF_RENDERING) {
		LOG_ERR("vulkan_cmd_bind_index_buffer: invalid command buffer state");
		return false;
	};
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	vk_buffer_data* buf_data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(buf_handle));

	if (!buf_data || !buf_data->buffer ||
		!(buf_data->usage_flags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
		LOG_ERR("vulkan_cmd_bind_index_buffer: invalid buffer");
		return false;
	}

	vkCmdBindIndexBuffer2(cmd_buf, buf_data->buffer, 0, VK_WHOLE_SIZE,
						  VK_INDEX_TYPE_UINT32);

	return true;
}

bool vulkan_copy_to_staging_buffer(gpu_api* self, gpu_buffer dst,
								   gpu_data_payload data) {
	vk_api_state* state = self->state;

	size_t size = 0;
	void* src = NULL;
	if (data.data_format == GPU_DATA_ARRAY) {
		size = u8_array_size(data.data.array);
		src = u8_array_begin(data.data.array);
		if (size == 0 || !src) {
			LOG_ERR("vulkan_copy_to_staging_buffer: invalid data payload");
			return false;
		}
	} else if (data.data_format == GPU_DATA_SPAN) {
		size = data.data.span.size;
		src = data.data.span.data;
		if (size == 0 || !data.data.span.data) {
			LOG_ERR("vulkan_copy_to_staging_buffer: invalid data payload");
			return false;
		}
	} else {
		ASSERT(false);
	}

	vk_buffer_data* dst_data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(dst));

	if (!dst_data || !dst_data->buffer ||
		!(dst_data->usage_flags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) ||
		!dst_data->alloc_info.pMappedData) {
		LOG_ERR("vulkan_copy_to_staging_buffer: invalid destination buffer");
		return false;
	}

	if (size > (dst_data->size - data.offset)) {
		LOG_ERR("vulkan_copy_to_staging_buffer: invalid transfer size");
		return false;
	}

	ASSERT(src);
	void* addr = (uint8_t*)dst_data->alloc_info.pMappedData + data.offset;
	memcpy(addr, src, size);

	return true;
}

bool vulkan_cmd_copy_to_buffer(gpu_api* self, gpu_command_buffer cmd,
							   gpu_buffer_copy_info* info) {
	vk_api_state* state = self->state;

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));
	if (cmd_data->state != CMDBUF_RECORDING) {
		LOG_ERR("vulkan_cmd_copy_to_buffer: invalid command buffer state");
		return false;
	};
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	vk_buffer_data* dst_data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(info->dst));

	if (!dst_data || !dst_data->buffer ||
		!(dst_data->usage_flags & VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
		LOG_ERR("vulkan_cmd_copy_to_buffer: invalid destination buffer");
		return false;
	}

	vk_buffer_data* src_data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(info->src));

	if (!src_data || !src_data->buffer ||
		!(src_data->usage_flags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) {
		LOG_ERR("vulkan_cmd_copy_to_buffer: invalid source buffer");
		return false;
	}

	if (!info->size) {
		LOG_ERR("vulkan_cmd_copy_to_buffer: invalid size");
		return false;
	}

	if (info->size > (dst_data->size - info->dst_offset) ||
		info->size > (src_data->size - info->src_offset)) {
		LOG_ERR("vulkan_cmd_copy_to_buffer: size is too big");
		return false;
	}

	VkBufferCopy2 reg = {0};
	{
		reg.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
		reg.size = info->size;
		reg.srcOffset = info->src_offset;
		reg.dstOffset = info->dst_offset;
	}

	VkCopyBufferInfo2 copy_ci = {0};
	{
		copy_ci.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
		copy_ci.srcBuffer = src_data->buffer;
		copy_ci.dstBuffer = dst_data->buffer;
		copy_ci.regionCount = 1;
		copy_ci.pRegions = &reg;
	}

	vkCmdCopyBuffer2(cmd_buf, &copy_ci);
	return true;
}

bool vulkan_cmd_buffer_barrier(gpu_api* self, gpu_command_buffer cmd,
							   gpu_buffer buf_handle, gpu_buffer_access from,
							   gpu_buffer_access to) {
	vk_api_state* state = self->state;

	vk_cmdbuf_data* cmd_data =
		vk_cmdbuf_pool_get(state->cmd_pool, to_vk_cmdbuf(cmd));
	if (cmd_data->state != CMDBUF_RECORDING) {
		LOG_ERR("vulkan_cmd_buffer_barrier: invalid command buffer state");
		return false;
	};
	VkCommandBuffer cmd_buf = cmd_data->command_buffer;

	vk_buffer_data* buf_data =
		vk_buffer_pool_get(state->buffer_pool, to_vk_buffer(buf_handle));

	if (!buf_data || !buf_data->buffer) {
		LOG_ERR("vulkan_cmd_buffer_barrier: invalid buffer");
		return false;
	}

	// [TODO]: validate that transition is valid, same as with images
	VkAccessFlags2 src_access =
		gpu_buffer_access_to_vk_state_flags[from].access;
	VkPipelineStageFlags2 src_stage =
		gpu_buffer_access_to_vk_state_flags[from].stage;
	VkAccessFlags2 dst_access = gpu_buffer_access_to_vk_state_flags[to].access;
	VkPipelineStageFlags2 dst_stage =
		gpu_buffer_access_to_vk_state_flags[to].stage;

	VkBufferMemoryBarrier2 barrier = {0};
	{
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		barrier.buffer = buf_data->buffer;
		barrier.size = VK_WHOLE_SIZE;

		barrier.srcAccessMask = src_access;
		barrier.srcStageMask = src_stage;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.dstAccessMask = dst_access;
		barrier.dstStageMask = dst_stage;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	VkDependencyInfo dep_info = {0};
	{
		dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dep_info.bufferMemoryBarrierCount = 1;
		dep_info.pBufferMemoryBarriers = &barrier;
	}
	vkCmdPipelineBarrier2(cmd_buf, &dep_info);

	return true;
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
		out->get_memory_info = vulkan_get_memory_info;
		out->wait_idle = vulkan_wait_idle;

		/* == Frame Flow ==================================================== */
		out->begin_frame = vulkan_begin_frame;
		out->cmd_begin_rendering = vulkan_cmd_begin_rendering;
		out->cmd_end_rendering = vulkan_cmd_end_rendering;
		out->cmd_set_viewport = vulkan_cmd_set_viewport;
		out->cmd_set_scissor = vulkan_cmd_set_scissor;
		out->cmd_reset_viewport = vulkan_cmd_reset_viewport;
		out->cmd_reset_scissor = vulkan_cmd_reset_scissor;
		out->cmd_draw = vulkan_cmd_draw;
		out->cmd_draw_indexed = vulkan_cmd_draw_indexed;
		out->cmd_draw_indirect = vulkan_cmd_draw_indirect;
		out->cmd_draw_indirect_indexed = vulkan_cmd_draw_indirect_indexed;
		out->end_frame = vulkan_end_frame;

		/* == Shaders & Pipelines ============================================*/
		out->create_shader = vulkan_create_shader;
		out->destroy_shader = vulkan_destroy_shader;
		out->create_pipeline = vulkan_create_pipeline;
		out->destroy_pipeline = vulkan_destroy_pipeline;
		out->cmd_bind_pipeline = vulkan_cmd_bind_pipeline;

		/* == Buffers ========================================================*/
		out->create_buffer = vulkan_create_buffer;
		out->destroy_buffer = vulkan_destroy_buffer;
		out->buffer_get_mapped = vulkan_map_buffer;
		out->buffer_get_device_address = vulkan_buffer_get_device_address;
		out->cmd_bind_index_buffer = vulkan_cmd_bind_index_buffer;
		out->copy_to_staging_buffer = vulkan_copy_to_staging_buffer;
		out->cmd_copy_to_buffer = vulkan_cmd_copy_to_buffer;
		out->cmd_buffer_barrier = vulkan_cmd_buffer_barrier;

		/* == Images =========================================================*/
		out->create_image = vulkan_create_image;
		out->destroy_image = vulkan_destroy_image;
		out->create_image_view = vulkan_create_image_view;
		out->get_image_view_shader_index = vulkan_get_image_view_shader_index;
		out->destroy_image_view = vulkan_destroy_image_view;
		out->cmd_copy_to_image = vulkan_cmd_copy_to_image;
		out->cmd_blit_to_image = vulkan_cmd_blit_to_image;
		out->cmd_transition_image = vulkan_cmd_transition_image;

		out->create_sampler = vulkan_create_sampler;
		out->destroy_sampler = vulkan_destroy_sampler;

		/* == Command Buffer and Sync ========================================*/
		out->create_command_buffer = vulkan_create_command_buffer;
		out->submit_command_buffer = vulkan_submit_command_buffer;
		out->submit_command_buffer_fence = vulkan_submit_command_buffer_fence;
		out->destroy_command_buffer = vulkan_destroy_command_buffer;

		out->create_fence = vulkan_create_fence;
		out->wait_fence = vulkan_wait_fence;
		out->destroy_fence = vulkan_destroy_fence;

		out->create_semaphore = NULL;
		out->destroy_semaphore = NULL;

		/* == Getters ======================================================= */
		out->get_framebuffer_extent = vulkan_get_framebuffer_extent;
		out->get_image_extent = vulkan_get_image_extent;
		out->get_image_format = vulkan_get_image_format;
	}

	return true;
}

gpu_pipeline gpu_pipeline_build(gpu_api* gpu, gpu_pipeline_builder* builder) {
	return vulkan_create_pipeline(gpu, &builder->ci);
}
