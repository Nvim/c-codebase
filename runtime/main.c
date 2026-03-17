#include <core/core.h>
#include <core/log.h>
#include <platform/platform.h>

#include <limits.h>

int main(void) {

	ASSERT(platform_init());
	window_size sz = {.width = 640, .height = 480};
	window_t* win = platform_create_window(sz, "hello");
	ASSERT(win != NULL);
	for (size_t i = 0; i < UINT_MAX && !platform_window_should_close(win); ++i) {
		LOG_INFO("%d", i);
		platform_poll();
	}
	platform_destroy_window(win);
	return 0;
}
