#include <core/core.h>
#include <core/log.h>
#include <filesystem/filesystem.h>
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

	arena_create_info arena_ci = {0};
	{
		arena_ci.initial_size = 8 * MEMORY_KIB;
		arena_ci.max_size = MEMORY_MIB;
		arena_ci.alignment = 8;
		arena_ci.flags = ARENA_LOG | ARENA_GROWING;
	}
	arena_t* arena = arena_create_from_config(arena_ci);
	ASSERT(arena);

	gpu_api gpu = {0};
	gpu_api_config gpu_ci = {0};
	{
		gpu_ci.app_name = "please work";
		gpu_ci.frames_in_flight = 3;
		gpu_ci.target_image_format = IMAGE_FORMAT_RGBA_SRGB;
		gpu_ci.type = GRAPHICS_API_VULKAN;
	}

	ASSERT(gpu_create_api(gpu_ci, &gpu));
	gpu.initialize(&gpu, win, arena, scratch.arena);
	ASSERT(gpu.is_init);

	arena_clear_scratch(scratch);

	double t_prev = platform_get_time();
	double delta = 0.0f;

	gpu_shader vs = {0}, fs = {0};
	{
		path vs_path;
		ASSERT(path_create(scratch.arena, scratch.arena,
						   str("src/shaders/quad.vert.spirv"), &vs_path));
		fs_file_read_result res = fs_read_all_file(scratch.arena, vs_path);
		ASSERT(res.ok);
		gpu_shader_ci vs_ci = {0};
		vs_ci.type = GPU_SHADER_VERTEX;
		vs_ci.code = res.bytes;
		vs_ci.debug_name = str("quad_vs");
		vs = gpu.create_shader(&gpu, &vs_ci);
	}
	{
		path fs_path;
		ASSERT(path_create(scratch.arena, scratch.arena,
						   str("src/shaders/quad.frag.spirv"), &fs_path));
		fs_file_read_result res = fs_read_all_file(scratch.arena, fs_path);
		ASSERT(res.ok);
		gpu_shader_ci fs_ci = {0};
		fs_ci.type = GPU_SHADER_VERTEX;
		fs_ci.code = res.bytes;
		fs_ci.debug_name = str("quad_fs");
		fs = gpu.create_shader(&gpu, &fs_ci);
	}
	arena_clear_scratch(scratch);

	ASSERT(check_handle(vs.handle));
	ASSERT(check_handle(fs.handle));

	gpu_pipeline_builder builder = {0};
	gpu_pipeline_builder_create(&builder);

	set_vertex_shader(&builder, vs);
	set_fragment_shader(&builder, fs);
	add_color_target(&builder, gpu.config.target_image_format, false);

	gpu_pipeline pipeline = gpu.create_pipeline(&gpu, &builder.ci);
	gpu.destroy_shader(&gpu, vs);
	gpu.destroy_shader(&gpu, fs);

	ASSERT(check_handle(pipeline.handle));
	gpu.destroy_pipeline(&gpu, pipeline);

	uint32_t frame_count = 0;
	while (!_quit) {
		if (frame_count++ > 12) { break; }
		platform_poll();
		if (platform_window_should_close(win)) { break; }
		gpu_frame_result frame_res = gpu.begin_frame(&gpu);
		ASSERT(frame_res.ok);
		gpu_frame frame = frame_res.frame;
		gpu_command_buffer cmd = frame.frame_cmd;

		gpu_color_target color_target = {
			.clear_color = (gpu_color){.floats = {.3f, .7f, .2f, 1.f}},
			.load_op = GPU_LOAD_OP_CLEAR,
			.store_op = GPU_STORE_OP_STORE,
			.target = frame.backbuffer,
		};
		gpu_render_state s = {
			.color_target_count = 1,
			.color_targets = {color_target},
		};
		gpu.cmd_transition_image(&gpu, cmd, frame.backbuffer,
								 GPU_USAGE_RENDER_TARGET);
		gpu.cmd_reset_viewport(&gpu, cmd);
		gpu.cmd_reset_scissor(&gpu, cmd);
		gpu.cmd_begin_rendering(&gpu, cmd, &s);
		gpu.cmd_end_rendering(&gpu, cmd);
		gpu.cmd_transition_for_present(&gpu, cmd, frame.backbuffer);
		gpu.cmd_submit(&gpu, cmd);
		gpu.end_frame(&gpu, &frame);
		double t_now = platform_get_time();
		delta = t_now - t_prev;
		if (delta < TARGET_FRAMETIME) {
			platform_sleep(TARGET_FRAMETIME - delta);
			LOG_DEBUG("frame took %f. sleeping for %f ms", delta,
					  TARGET_FRAMETIME - delta);
			LOG_DEBUG("==============================================");
		}
		t_now = platform_get_time();
		t_prev = t_now;
	}

	platform_destroy_window(win);

	gpu.destroy(&gpu);

	// platform_destroy();
	return 0;
}
