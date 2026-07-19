#include <platform/platform.h>

#include <time.h>
#include <unistd.h>

LIB_EXPORT void platform_sleep(double time_ms) { usleep(time_ms * 1000); }

LIB_EXPORT double platform_get_time(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double time = now.tv_sec + now.tv_nsec * 0.000000001;
	return time * 1000.0f;
}

LIB_EXPORT double platform_get_time_s(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}
