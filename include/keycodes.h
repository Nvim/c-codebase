#ifndef KEYCODES_H
#define KEYCODES_H

#include "keycodes_list.h"

typedef enum INPUT_Key {
	INPUT_KEY_UNKNOWN = -1,

#define X(name) INPUT_KEY_##name,
	INPUT_KEYCODE_LIST
#undef X

		INPUT_KEY_COUNT
} input_key;

typedef enum input_mouse_button {
	INPUT_BUTTON_UNKNOWN = -1,

#define X(name) INPUT_BUTTON_##name,
	INPUT_BUTTON_LIST
#undef X

		INPUT_BUTTON_COUNT
} input_mouse_button;

input_key input_translate_internal_keycode(int internal_key);
input_mouse_button input_translate_internal_button(int internal_key);

#endif // !KEYCODES_H
