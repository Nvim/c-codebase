#include <core/core.h>
#include <core/log.h>
#include <platform/platform.h>

#include <graphics/graphics.h>
#include <limits.h>

static bool _quit = false;
static double const TARGET_FRAMETIME = (1 / 60.f) * 1000.f;

int main(void) {

	ASSERT(platform_init());
	window_size sz = {.width = 640, .height = 480};
	window_t* win = platform_create_window(sz, "hello");
	ASSERT(win != NULL);

	ASSERT(arena_pool_init());
	scratch_arena scratch = {0};
	ASSERT(arena_get_scratch(NULL, &scratch));

	arena_t* arena = arena_create(4096);

	gpu_api gpu = {0};
	gpu_api_config gpu_ci = {0};
	{
		gpu_ci.app_name = "please work";
		gpu_ci.frames_in_flight = 2;
		gpu_ci.target_image_format = IMAGE_FORMAT_RGBA_SRGB;
		gpu_ci.type = GRAPHICS_API_VULKAN;
	}

	ASSERT(gpu_create_api(gpu_ci, &gpu));
	gpu.initialize(&gpu, win, arena, scratch.arena);
	ASSERT(gpu.is_init);

	arena_clear_scratch(scratch);

	double t_prev = platform_get_time();
	double delta = 0.0f;

	while (!_quit) {
		platform_poll();
		if (platform_window_should_close(win)) { break; }
		gpu.render(&gpu);
		double t_now = platform_get_time();
		delta = t_now - t_prev;
		if (delta < TARGET_FRAMETIME) {
			platform_sleep(TARGET_FRAMETIME - delta);
			LOG_DEBUG("frame took %f. sleeping for %f ms", delta,
					  TARGET_FRAMETIME - delta);
		}
		t_now = platform_get_time();
		t_prev = t_now;
	}

	platform_destroy_window(win);

	gpu.destroy(&gpu);

	// platform_destroy();
	return 0;
}
