#include <core/core.h>
#include <core/log.h>
#include <platform/platform.h>

#include <limits.h>

static bool _quit = false;

void press_callback(UNUSED window_t* w, UNUSED input_key key,
					UNUSED key_action action) {
	LOG_WARN("quitting when you release");
}
void release_callback(UNUSED window_t* w, UNUSED input_key key,
					  UNUSED key_action action) {
	LOG_WARN("quitting in 3 seconds");
	platform_sleep(3000);
	_quit = true;
}

void all_action_callback(UNUSED window_t* w, UNUSED input_key key,
						 UNUSED key_action action) {
	LOG_INFO("hey look it's the same message");
}

void mouse_btn_callback(UNUSED window_t* w, UNUSED input_mouse_button btn,
						UNUSED key_action action) {
	char const* act = NULL;
	switch (action) {
	case INPUT_ACTION_HELD: act = "held"; break;
	case INPUT_ACTION_PRESSED: act = "pressed"; break;
	case INPUT_ACTION_RELEASED: act = "released"; break;
	default: break;
	}
	LOG_INFO("[main]: button %d was %s", btn, act);
}

static double const TARGET_FRAMETIME = (1 / 60.f) * 1000.f;

int main(void) {

	ASSERT(platform_init(PLATFORM_TYPE_LINUX_X11));
	window_size sz = {.width = 640, .height = 480};
	window_t* win = platform_create_window(sz, "hello");
	ASSERT(win != NULL);

	platform_register_key_callback(INPUT_KEY_ESCAPE, INPUT_ACTION_PRESSED,
								   press_callback);
	platform_register_key_callback(INPUT_KEY_ESCAPE, INPUT_ACTION_RELEASED,
								   release_callback);
	platform_register_key_callback(INPUT_KEY_A,
								   INPUT_ACTION_RELEASED | INPUT_ACTION_HELD |
									   INPUT_ACTION_PRESSED,
								   all_action_callback);

	platform_register_mouse_button_callback(
		INPUT_BUTTON_LEFT_CLICK, INPUT_ACTION_PRESSED | INPUT_ACTION_RELEASED,
		mouse_btn_callback);

	double t_prev = platform_get_time();
	double delta = 0.0f;

	while (!_quit) {
		platform_poll();
		if (platform_window_should_close(win)) { break; }
		double t_now = platform_get_time();
		delta = t_now - t_prev;
		if (delta < TARGET_FRAMETIME) {
			platform_sleep(TARGET_FRAMETIME - delta);
			// LOG_DEBUG("frame took %f. sleeping for %f ms", delta,
			// TARGET_FRAMETIME - delta);
		}
		t_now = platform_get_time();
		t_prev = t_now;
	}

	platform_destroy_window(win);

	// [TODO]: glfw crash
	platform_destroy();
	return 0;
}
