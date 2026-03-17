#include <platform/platform.h>

#include <GLFW/glfw3.h>
#include "input.h"

#include <stdio.h>

void on_error(int code, char const* description) {
	fprintf(stderr, "[GLFW] Error %d: %s", code, description);
}

window_t* platform_create_window(window_size size, char const* name) {
	glfwSetErrorCallback(on_error);
	if (glfwInit() != GLFW_TRUE) {
		assert(false);
		return NULL;
	}
	if (glfwVulkanSupported() != GLFW_TRUE) {
		assert(false);
		return NULL;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	return (window_t*)glfwCreateWindow(size.width, size.height, name, NULL,
									   NULL);
}

window_size platform_get_window_size(window_t* window) {
	window_size ret = {0};
	glfwGetFramebufferSize((GLFWwindow*)window, &ret.width, &ret.height);
	return ret;
}

bool platform_set_window_name(window_t* window, char const* name) {
	glfwSetWindowTitle((GLFWwindow*)window, name);
	return true;
}

char const* platform_get_window_name(window_t* window) {
	return glfwGetWindowTitle((GLFWwindow*)window);
}

void platform_destroy_window(window_t* window) {
	glfwDestroyWindow((GLFWwindow*)window);
	glfwTerminate();
}
