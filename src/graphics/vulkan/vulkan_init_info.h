#ifndef VULKAN_INIT_INFO
#define VULKAN_INIT_INFO

#include <core/core.h>
#include <platform/platform.h>

#include <vulkan/vulkan_core.h>

// Platform needs to provide these in order to setup Vulkan API.
typedef struct vulkan_init_info {
	char const** required_instance_extensions;
	uint32_t required_instance_extensions_count;
	int (*can_device_queue_present)(VkInstance instance,
									VkPhysicalDevice device, uint32_t qf_idx);
	VkSurfaceKHR (*create_surface)(window_t* window, VkInstance instance);
} vulkan_init_info;

STATIC_ASSERT(sizeof(vulkan_init_info) == 32, "");

#endif // !VULKAN_INIT_INFO
