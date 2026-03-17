#include <limits.h>
#include <platform/platform.h>
#include <core/log.h>

int main(void) {

	window_size sz = {.width = 640, .height = 480};
	window_t* win = platform_create_window(sz, "hello");
	for(size_t i = 0; i < UINT_MAX; ++i) {
		LOG_INFO("%d", i);
	}
	platform_destroy_window(win);
	return 0;
}
