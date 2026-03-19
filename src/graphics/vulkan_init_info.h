#ifndef VULKAN_INIT_INFO
#define VULKAN_INIT_INFO

#include <platform/platform.h>

#include <vulkan/vulkan_core.h>

// Platform needs to provide these in order to setup Vulkan API.
typedef struct vulkan_init_info {
	const char** required_instance_extensions;
	uint32_t required_instance_extensions_count;
	VkSurfaceKHR (*create_surface)(window_t* window, VkInstance instance);
} vulkan_init_info;

#endif // !VULKAN_INIT_INFO
