#ifndef KEYCODES_H
#define KEYCODES_H

#include "keycodes_list.h"

typedef enum INPUT_Key
{
    INPUT_KEY_UNKNOWN = -1,

#define X(name) INPUT_KEY_##name,
	INPUT_KEYCODE_LIST
#undef X

    INPUT_KEY_COUNT
} input_key;

input_key input_translate_internal_keycode(int internal_key);

#endif // !KEYCODES_H
