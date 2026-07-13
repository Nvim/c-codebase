/**
 * @file
 * @brief Implements Window and Input API with GLFW.
 *
 * @note Platform init and shutdown are also handled here
 */
#include <vulkan/vulkan.h>

#include "input.h"

#include <core/core.h>
#include <graphics/vulkan/vulkan_init_info.h>
#include <platform/platform.h>

#include <GLFW/glfw3.h>
#include <string.h>

// Temporary debug flag so I can see a window before getting Vulkan working
// #define CREATE_OPENGL_CONTEXT 1

typedef struct window_impl {
	GLFWwindow* internal;
} window_impl;

void on_error(int code, char const* description) {
	LOG_ERR("[GLFW] Error %d: %s", code, description);
}

static key_action const key_act_types[] = {
	INPUT_ACTION_PRESSED, INPUT_ACTION_RELEASED, INPUT_ACTION_HELD};

static key_action const mouse_act_types[] = {INPUT_ACTION_PRESSED,
											 INPUT_ACTION_RELEASED};

static bool _init = false;
static bool _key_state[INPUT_KEY_COUNT] = {0};
static bool _mouse_state[INPUT_BUTTON_COUNT] = {0};

typedef struct key_cb_entry {
	key_callback callbacks[INPUT_ACTION_COUNT];
} key_cb_entry;

typedef struct mouse_button_cb_entry {
	mouse_button_callback callbacks[INPUT_ACTION_COUNT];
} mouse_button_cb_entry;

static key_cb_entry _key_callbacks[INPUT_KEY_COUNT] = {0};
static mouse_button_cb_entry _mouse_button_callbacks[INPUT_KEY_COUNT] = {0};

static void _key_callback(GLFWwindow* window, int key, int scancode, int action,
						  int mods);
static void _mouse_button_callback(GLFWwindow* window, int button, int action,
								   int mods);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static void cursor_position_callback(GLFWwindow* window, double xpos,
									 double ypos);
static void framebuffer_size_callback(GLFWwindow* window, int width,
									  int height);

bool platform_init(void) {
	glfwSetErrorCallback(on_error);

	glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
	_init = (glfwInit() == GLFW_TRUE);
	ASSERT(_init);

	return _init;
}

// [NOTE] glfwTerminate crashes on Wayland
LIB_EXPORT void platform_destroy(void) { /* glfwTerminate(); */ }

window_t* platform_create_window(window_size size, char const* name) {
	// [TODO]: Someday another API might be supported who knows
	if (glfwVulkanSupported() != GLFW_TRUE) {
		ASSERT(false);
		return NULL;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window =
		glfwCreateWindow(size.width, size.height, name, NULL, NULL);

	glfwSetKeyCallback(window, _key_callback);
	glfwSetMouseButtonCallback(window, _mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	window_impl* ret = (window_impl*)window;
	ret->internal = window;
	ASSERT(ret->internal == window);
	return (window_t*)ret;
}

window_size platform_get_window_size(window_t* window) {
	window_size ret = {0};
	window_impl* w = (window_impl*)window;
	glfwGetFramebufferSize(w->internal, &ret.width, &ret.height);
	return ret;
}

bool platform_set_window_name(window_t* window, char const* name) {
	window_impl* w = (window_impl*)window;
	glfwSetWindowTitle(w->internal, name);
	return true;
}

char const* platform_get_window_name(window_t* window) {
	window_impl* w = (window_impl*)window;
	return glfwGetWindowTitle(w->internal);
}

bool platform_window_should_close(window_t* window) {
	window_impl* w = (window_impl*)window;
	return glfwWindowShouldClose(w->internal);
}

void platform_destroy_window(UNUSED window_t* window) {
	window_impl* w = (window_impl*)window;
	glfwDestroyWindow(w->internal);
}

void platform_poll(void) { glfwPollEvents(); }

void platform_register_key_callback(input_key key, key_action action,
									key_callback callback) {
	ASSERT(_init);
	key_cb_entry* entry = &_key_callbacks[key];
	ASSERT(entry);

	for (int i = 0; i < 3; ++i) {
		if (action & key_act_types[i]) {
			if (entry->callbacks[key_act_types[i]]) {
				LOG_WARN("platform_register_key_callback: overriding an "
						 "existing key "
						 "callback");
			}
			entry->callbacks[key_act_types[i]] = callback;
		}
	}
}

void platform_register_mouse_button_callback(input_mouse_button button,
											 key_action action,
											 mouse_button_callback callback) {
	ASSERT(_init);
	mouse_button_cb_entry* entry = &_mouse_button_callbacks[button];
	ASSERT(entry);

	// [NOTE]: works because pressed and released are defined as 1 and 2
	for (int i = 0; i < 2; ++i) {
		if (action & mouse_act_types[i]) {
			if (entry->callbacks[mouse_act_types[i]]) {
				LOG_WARN(
					"platform_register_mouse_button_callback: overriding an "
					"existing button callback");
			}
			entry->callbacks[mouse_act_types[i]] = callback;
		}
	}
}

static VkSurfaceKHR _create_vk_surface(window_t* window, VkInstance instance);

bool platform_init_graphics_api(gpu_api_type type,
								gpu_api_platform_init_info* out) {
	if (type != GRAPHICS_API_VULKAN) { return false; }

	if (glfwVulkanSupported() != GLFW_TRUE) { return false; }

	vulkan_init_info* ret = (vulkan_init_info*)out;
	memset(ret, 0, sizeof(vulkan_init_info));

	char const** required_instance_extensions;
	uint32_t required_instance_extensions_count = 0;
	required_instance_extensions =
		glfwGetRequiredInstanceExtensions(&required_instance_extensions_count);
	if (required_instance_extensions == NULL ||
		required_instance_extensions_count == 0) {
		return false;
	}

	ret->required_instance_extensions = required_instance_extensions;
	ret->required_instance_extensions_count =
		required_instance_extensions_count;

	ret->create_surface = _create_vk_surface;
	ret->can_device_queue_present = glfwGetPhysicalDevicePresentationSupport;

	return true;
}

static void _key_callback(GLFWwindow* window, int key, UNUSED int scancode,
						  int action, UNUSED int mods) {
	input_key translated = input_translate_internal_keycode(key);
	key_cb_entry* e = NULL;
	e = &_key_callbacks[translated];
	switch (action) {
	case GLFW_REPEAT:
		LOG_DEBUG("key %d (GLFW %d) was repeated.", translated, key);
		if (e && e->callbacks[INPUT_ACTION_HELD]) {
			e->callbacks[INPUT_ACTION_HELD]((window_t*)window, translated,
											INPUT_ACTION_HELD);
		}
		break;
	case GLFW_PRESS:
		_key_state[translated] = true;
		LOG_DEBUG("key %d (GLFW %d) was pressed.", translated, key);
		if (e && e->callbacks[INPUT_ACTION_PRESSED]) {
			e->callbacks[INPUT_ACTION_PRESSED]((window_t*)window, translated,
											   INPUT_ACTION_PRESSED);
		}
		break;
	case GLFW_RELEASE:
		_key_state[translated] = false;
		LOG_DEBUG("key %d (GLFW %d) was released.", translated, key);
		if (e && e->callbacks[INPUT_ACTION_RELEASED]) {
			e->callbacks[INPUT_ACTION_RELEASED]((window_t*)window, translated,
												INPUT_ACTION_RELEASED);
		}
		break;
	default: ASSERT(false); break;
	}
}

static void _mouse_button_callback(GLFWwindow* window, int button, int action,
								   UNUSED int mods) {
	input_mouse_button translated = input_translate_internal_button(button);
	mouse_button_cb_entry* e = &_mouse_button_callbacks[translated];
	switch (action) {
	case GLFW_REPEAT:
		LOG_DEBUG("button %d (GLFW %d) was repeated.", translated, button);
		if (e && e->callbacks[INPUT_ACTION_HELD]) {
			e->callbacks[INPUT_ACTION_HELD]((window_t*)window, translated,
											INPUT_ACTION_HELD);
		}
		break;
	case GLFW_PRESS:
		_mouse_state[translated] = true;
		LOG_DEBUG("button %d (GLFW %d) was pressed.", translated, button);
		if (e && e->callbacks[INPUT_ACTION_PRESSED]) {
			e->callbacks[INPUT_ACTION_PRESSED]((window_t*)window, translated,
											   INPUT_ACTION_PRESSED);
		}
		break;
	case GLFW_RELEASE:
		_mouse_state[translated] = false;
		LOG_DEBUG("button %d (GLFW %d) was released.", translated, button);
		if (e && e->callbacks[INPUT_ACTION_RELEASED]) {
			e->callbacks[INPUT_ACTION_RELEASED]((window_t*)window, translated,
												INPUT_ACTION_RELEASED);
		}
		break;
	default: ASSERT(false); break;
	};
}

static void cursor_position_callback(GLFWwindow* window, double xpos,
									 double ypos) {
	// TODO
	(void)window;
	(void)xpos;
	(void)ypos;
}

static void scroll_callback(GLFWwindow* window, double xoffset,
							double yoffset) {
	// TODO
	(void)window;
	(void)xoffset;
	(void)yoffset;
}

static void framebuffer_size_callback(GLFWwindow* window, int width,
									  int height) {
	// TODO
	(void)window;
	(void)width;
	(void)height;
	LOG_WARN("glfw_framebuffer_size_callback: window was resized")
}

static VkSurfaceKHR _create_vk_surface(window_t* window, VkInstance instance) {
	VkSurfaceKHR ret = {0};

	window_impl* win = (window_impl*)window;
	VkResult result =
		glfwCreateWindowSurface(instance, win->internal, NULL, &ret);
	ASSERT(result == VK_SUCCESS && ret != NULL);

	return ret;
}
