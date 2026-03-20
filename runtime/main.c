#include <core/core.h>
#include <core/log.h>
#include <platform/platform.h>

#include <limits.h>

static bool _quit = false;

void press_callback(void* _) { LOG_WARN("quitting when you release"); }
void release_callback(void* _) {
	LOG_WARN("quitting in 3 seconds");
	platform_sleep(3000);
	_quit = true;
}

void all_action_callback(void* _) {
	LOG_INFO("hey look it's the same message");
}

static double const TARGET_FRAMETIME = (1 / 60.f) * 1000.f;

int main(void) {

	ASSERT(platform_init());
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
	// platform_destroy();
	return 0;
}
