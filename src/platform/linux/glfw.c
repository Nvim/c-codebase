/**
 * @file
 * @brief Implements Window and Input API with GLFW.
 *
 * @note Platform init and shutdown are also handled here
 */
#include "input.h"

#include <core/core.h>
#include <graphics/vulkan_init_info.h>
#include <platform/platform.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string.h>

// Temporary debug flag so I can see a window before getting Vulkan working
#define CREATE_OPENGL_CONTEXT 1

typedef struct window_impl {
	GLFWwindow* internal;
} window_impl;

void on_error(int code, char const* description) {
	LOG_ERR("[GLFW] Error %d: %s", code, description);
}

static bool _init = false;
static bool _key_state[INPUT_KEY_COUNT] = {0};
// [TODO]: serialize whole frames someday
static bool _prev_key_state[INPUT_KEY_COUNT] = {0};

typedef struct key_cb_entry {
	key_callback callbacks[INPUT_ACTION_COUNT];
} key_cb_entry;

static key_cb_entry _key_callbacks[INPUT_KEY_COUNT] = {0};

// [TODO]: Only needed for glfwSwapBuffers call. Remove when Vulkan is setup
static GLFWwindow* _window = NULL;

static void _key_callback(GLFWwindow* window, int key, int scancode, int action,
						  int mods);
static void mouse_button_callback(GLFWwindow* window, int button, int action,
								  int mods);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static void cursor_position_callback(GLFWwindow* window, double xpos,
									 double ypos);
static void framebuffer_size_callback(GLFWwindow* window, int width,
									  int height);

bool platform_init(void) {
	glfwSetErrorCallback(on_error);

	_init = (glfwInit() == GLFW_TRUE);
	ASSERT(_init);

	return _init;
}

LIB_EXPORT void platform_destroy(void) { glfwTerminate(); }

window_t* platform_create_window(window_size size, char const* name) {
	if (glfwVulkanSupported() != GLFW_TRUE) {
		ASSERT(false);
		return NULL;
	}

#if defined(CREATE_OPENGL_CONTEXT)
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

	GLFWwindow* window =
		glfwCreateWindow(size.width, size.height, name, NULL, NULL);

	glfwSetKeyCallback(window, _key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

#if defined(CREATE_OPENGL_CONTEXT)
	glfwMakeContextCurrent(window);
	_window = window;
#endif

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

void platform_destroy_window(window_t* window) {
	window_impl* w = (window_impl*)window;
	glfwDestroyWindow(w->internal);
}

void platform_poll(void) {
	memcpy(_prev_key_state, _key_state, INPUT_KEY_COUNT);
#if defined(CREATE_OPENGL_CONTEXT)
	glfwSwapBuffers(_window);
#endif
	glfwPollEvents();
}

LIB_EXPORT void platform_register_key_callback(input_key key, key_action action,
											   key_callback callback) {
	ASSERT(_init);
	key_cb_entry* entry = &_key_callbacks[key];
	ASSERT(entry);

	static key_action const act_types[] = {INPUT_ACTION_PRESSED,
										   INPUT_ACTION_RELEASED, INPUT_ACTION_HELD};
	for (int i = 0; i < 3; ++i) {
		if (action & act_types[i]) {
			if (entry->callbacks[act_types[i]]) {
				LOG_WARN("platform_register_key_callback: overriding an "
						 "existing key "
						 "callback");
			}
			entry->callbacks[act_types[i]] = callback;
		}
	}
}

static VkSurfaceKHR _create_vk_surface(window_t* window, VkInstance instance);

// bool platform_init_graphics_api(graphics_api_type type, graphics_api* out) {
bool platform_init_graphics_api(graphics_api_type type,
								graphics_api_init_info* out) {
	if (type != GRAPHICS_API_VULKAN) { return false; }

	if (glfwVulkanSupported() != GLFW_TRUE) { return false; }

	vulkan_init_info* ret = (vulkan_init_info*)out;
	memset(ret, 0, sizeof(vulkan_init_info));

	char const** required_instance_extensions;
	uint32_t required_instance_extensions_count;
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

	return true;
}

static void _key_callback(GLFWwindow* window, int key, int scancode, int action,
						  int mods) {
	(void)window;
	(void)scancode;
	(void)mods;
	input_key translated = input_translate_internal_keycode(key);
	key_cb_entry* e = NULL;
	switch (action) {
	case GLFW_REPEAT:
		LOG_DEBUG("key %d (GLFW %d) was repeated.", translated, key);
		e = &_key_callbacks[translated];
		if (e && e->callbacks[INPUT_ACTION_HELD]) {
			e->callbacks[INPUT_ACTION_HELD](NULL);
		}
		break;
	case GLFW_PRESS:
		_key_state[translated] = true;
		LOG_DEBUG("key %d (GLFW %d) was pressed.", translated, key);
		e = &_key_callbacks[translated];
		if (e && e->callbacks[INPUT_ACTION_PRESSED]) {
			e->callbacks[INPUT_ACTION_PRESSED](NULL);
		}
		break;
	case GLFW_RELEASE:
		_key_state[translated] = false;
		LOG_DEBUG("key %d (GLFW %d) was released.", translated, key);
		e = &_key_callbacks[translated];
		if (e && e->callbacks[INPUT_ACTION_RELEASED]) {
			e->callbacks[INPUT_ACTION_RELEASED](NULL);
		}
		break;
	default: ASSERT(false); break;
	}
}

static void mouse_button_callback(GLFWwindow* window, int button, int action,
								  int mods) {
	(void)window;
	(void)action;
	(void)mods;
	// TODO
	switch (button) {
	case GLFW_MOUSE_BUTTON_RIGHT: break;
	case GLFW_MOUSE_BUTTON_LEFT: break;
	case GLFW_MOUSE_BUTTON_MIDDLE: break;
	}
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
}

static VkSurfaceKHR _create_vk_surface(window_t* window, VkInstance instance) {
	VkSurfaceKHR ret = {0};

	window_impl* win = (window_impl*)window;
	VkResult result =
		glfwCreateWindowSurface(instance, win->internal, NULL, &ret);
	ASSERT(result == VK_SUCCESS && ret != NULL);

	return ret;
}
