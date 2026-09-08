#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include <keycodes.h>

#include <stdbool.h>

typedef struct engine_input_state {
	bool pressed_keys[INPUT_KEY_COUNT];	 // Was just pressed this frame
	bool held_keys[INPUT_KEY_COUNT];	 // Was pressed, still not released
	bool released_keys[INPUT_KEY_COUNT]; // Released this frame

	bool pressed_buttons[INPUT_BUTTON_COUNT]; // Was just pressed this frame
	bool held_buttons[INPUT_BUTTON_COUNT];	  // Was pressed, still not released
	bool released_buttons[INPUT_BUTTON_COUNT]; // Released this frame
} engine_input_state;

// TODO: move engine internal functions out of public header. Only types should
// remain.

/**
 * @brief Initialize engine input system. Must be called before other functions.
 * Assumes `platform_init` was called beforehand.
 */
bool engine_input_init(void);

/**
 * @brief Shutdown engine input system. No other functions should be called
 * after this.
 */
void engine_input_shutdown(void);

/**
 * @brief Get input state for current frame. Doesn't perform any update,
 * `engine_input_update` should be called first.
 *
 */
engine_input_state const* engine_input_get_state(void);

/**
 * @brief Update input state for this frame. Assumes `platform_poll` was called
 * beforehand.
 */
engine_input_state const* engine_input_update(void);

#endif // !ENGINE_INPUT_H
