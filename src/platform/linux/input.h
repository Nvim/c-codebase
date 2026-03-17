#include <keycodes.h>
#include <keycodes_list.h>

#include <GLFW/glfw3.h>

input_key input_translate_internal_keycode(int key) {
    switch (key)
    {
#define X(name) case GLFW_KEY_##name: return INPUT_KEY_##name;
		INPUT_KEYCODE_LIST
#undef X

        default: return INPUT_KEY_UNKNOWN;
    }
}
