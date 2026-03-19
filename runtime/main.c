#include <core/core.h>
#include <core/log.h>
#include <platform/platform.h>

#include <limits.h>

static bool _quit = false;

void press_callback(void* _) {
	LOG_WARN("quitting when you release");
}
void release_callback(void* _) {
	LOG_WARN("quitting..");
	_quit = true;
}

int main(void) {

	ASSERT(platform_init());
	window_size sz = {.width = 640, .height = 480};
	window_t* win = platform_create_window(sz, "hello");
	ASSERT(win != NULL);

	platform_register_key_callback(INPUT_KEY_ESCAPE, INPUT_KEY_PRESSED,
								   press_callback);
	platform_register_key_callback(INPUT_KEY_ESCAPE, INPUT_KEY_RELEASED,
								   release_callback);

	while (!_quit) {
		platform_poll();
		if (platform_window_should_close(win)) { break; }
	}
	platform_destroy_window(win);
	return 0;
}
