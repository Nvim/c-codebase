#include <engine/input.h>

#include <core/core.h>
#include <keycodes.h>
#include <platform/platform.h>
#include <string.h>

static bool _init = false;
static engine_input_state _input_state = {0};

static platform_keyboard_state _previous_key_state = {0};
static platform_keyboard_state _current_key_state = {0};

static platform_mouse_state _previous_mouse_state = {0};
static platform_mouse_state _current_mouse_state = {0};

bool engine_input_init(void) {
	if (_init) return false;

	_init = true;
	return true;
}

void engine_input_shutdown(void) { _init = false; }

engine_input_state const* engine_input_get_state(void) {
	ASSERT(_init);
	return &_input_state;
}

static bool _was_key_down(input_key key) {
	return _previous_key_state.pressed_keys[key];
}

static bool _was_button_down(input_mouse_button button) {
	return _previous_mouse_state.pressed_buttons[button];
}

static bool _is_key_down(input_key key) {
	return _current_key_state.pressed_keys[key];
}

static bool _is_button_down(input_mouse_button button) {
	return _current_mouse_state.pressed_buttons[button];
}

engine_input_state const* engine_input_update(void) {

	ASSERT(_init);

	_previous_key_state = _current_key_state;
	platform_get_keyboard_state(&_current_key_state);

	// Reset released & pressed (single frame)
	memset(_input_state.released_buttons, 0,
		   sizeof(_input_state.released_buttons));
	memset(_input_state.pressed_buttons, 0,
		   sizeof(_input_state.pressed_buttons));
	memset(_input_state.pressed_keys, 0, sizeof(_input_state.pressed_keys));
	memset(_input_state.released_keys, 0, sizeof(_input_state.released_keys));

	// Compute held & released keys
	for (uint32_t i = 0; i < INPUT_KEY_COUNT; ++i) {
		if (_was_key_down(i)) {
			if (_is_key_down(i)) {
				_input_state.held_keys[i] = true;
			} else {
				// Key was just released
				_input_state.held_keys[i] = false;
				_input_state.released_keys[i] = true;
			}
		}
		if (_is_key_down(i)) { _input_state.pressed_keys[i] = true; }
	}

	// Compute held & released buttons
	for (uint32_t i = 0; i < INPUT_BUTTON_COUNT; ++i) {
		if (_was_button_down(i)) {
			if (_is_button_down(i)) {
				_input_state.held_buttons[i] = true;
			} else {
				// button was just released
				_input_state.held_buttons[i] = false;
				_input_state.released_buttons[i] = true;
			}
		}
		if (_is_button_down(i)) { _input_state.pressed_buttons[i] = true; }
	}

	return &_input_state;
}
