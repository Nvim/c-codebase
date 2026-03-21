#include <keycodes.h>
#include <keycodes_list.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

input_key input_translate_internal_keycode(int key) {
	switch (key) {
#define X(name)                                                                \
	case GLFW_KEY_##name: return INPUT_KEY_##name;
		INPUT_KEYCODE_LIST
#undef X

	default: return INPUT_KEY_UNKNOWN;
	}
}

input_mouse_button input_translate_internal_button(int btn) {
	switch (btn) {
	case GLFW_MOUSE_BUTTON_LEFT: return INPUT_BUTTON_LEFT_CLICK;
	case GLFW_MOUSE_BUTTON_RIGHT: return INPUT_BUTTON_RIGHT_CLICK;
	case GLFW_MOUSE_BUTTON_MIDDLE: return INPUT_BUTTON_MIDDLE_CLICK;

	default: return INPUT_BUTTON_UNKNOWN;
	}
}
