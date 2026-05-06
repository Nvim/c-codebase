#include <core/core.h>
#include <core/log.h>
#include <filesystem/filesystem.h>
#include <platform/platform.h>

#include <graphics/graphics.h>
#include <limits.h>
#include <string.h>

static bool _quit = false;
static double const TARGET_FRAMETIME = (1 / 60.f) * 1000.f;

static uint8_t const WHITE[4] = {0xff, 0xff, 0xff, 0xff};
static uint8_t const RED[4] = {0xff, 0x0, 0x0, 0xff};
static uint8_t const GREEN[4] = {0x0, 0xff, 0x0, 0xff};
static uint8_t const BLUE[4] = {0x0, 0x0, 0xff, 0xff};

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
	{ // Vertex shader
		path vs_path;
		ASSERT(path_create(arena, scratch.arena,
						   str("src/shaders/quad.vert.spirv"), &vs_path));
		fs_file_read_result res = fs_read_all_file(arena, vs_path);
		ASSERT(res.ok);
		gpu_shader_ci vs_ci = {0};
		vs_ci.type = GPU_SHADER_VERTEX;
		vs_ci.code = res.bytes;
		vs_ci.debug_name = str("quad_vs");
		vs = gpu.create_shader(&gpu, &vs_ci);
		ASSERT(check_handle(vs.handle));
	}
	{ // Fragment shader
		path fs_path;
		ASSERT(path_create(arena, scratch.arena,
						   str("src/shaders/quad.frag.spirv"), &fs_path));
		fs_file_read_result res = fs_read_all_file(arena, fs_path);
		ASSERT(res.ok);
		gpu_shader_ci fs_ci = {0};
		fs_ci.type = GPU_SHADER_FRAGMENT;
		fs_ci.code = res.bytes;
		fs_ci.debug_name = str("quad_fs");
		fs = gpu.create_shader(&gpu, &fs_ci);
		ASSERT(check_handle(fs.handle));
	}
	arena_clear_scratch(scratch);

	gpu_pipeline pipeline;
	{ // Pipeline
		gpu_pipeline_builder builder = {0};
		gpu_pipeline_builder_create(&builder);
		set_vertex_shader(&builder, vs);
		set_fragment_shader(&builder, fs);
		add_color_target(&builder, gpu.config.target_image_format, false);
		pipeline = gpu_pipeline_build(&gpu, &builder);
		ASSERT(check_handle(pipeline.handle));
	}

	gpu.destroy_shader(&gpu, vs);
	gpu.destroy_shader(&gpu, fs);

	gpu_image test_image;
	{ // [TODO]: temp
		gpu_image_ci img_ci = {0};
		img_ci.dimensions = (gpu_image_extent){.width = 2, .height = 2};
		img_ci.format = IMAGE_FORMAT_RGBA_UNORM;
		img_ci.type = GPU_IMAGE_TYPE_2D;
		img_ci.usage = GPU_IMAGE_USAGE_SAMPLED;
		img_ci.array_layers = 1;
		img_ci.mip_levels = 1;
		test_image = gpu.create_image(&gpu, &img_ci);
		ASSERT(check_handle(test_image.handle));
	}
	gpu_image_view full_view;
	ASSERT(gpu.get_image_full_view(&gpu, &full_view, test_image));
	ASSERT(full_view.internal != NULL);

	u8_array pixels = u8_array_create(arena, 16);
	uint8_t* px = u8_array_begin(pixels);
	{
		memcpy(px, RED, 4);
		memcpy(px + 4, GREEN, 4);
		memcpy(px + 8, BLUE, 4);
		memcpy(px + 12, WHITE, 4);
	}

	image_data const test_image_data = {
		.height = 2,
		.width = 2,
		.bytes = pixels,
	};

	uint32_t frame_count = 0;
	while (!_quit) {
		if (frame_count++ > 12) { break; }

		platform_poll();
		if (platform_window_should_close(win)) { break; }

		gpu_frame_result frame_res = gpu.begin_frame(&gpu);
		ASSERT(frame_res.ok);
		gpu_frame frame = frame_res.frame;
		gpu_command_buffer cmd = frame.frame_cmd;

		// Copy pixel data to image on 1st frame only
		if (frame_count == 1) {
			gpu.cmd_transition_image(&gpu, cmd, test_image,
									 GPU_IMAGE_STATE_TRANSFER_DST);
			ASSERT(
				gpu.temp_copy_to_image(&gpu, cmd, test_image, test_image_data));
			gpu.cmd_transition_image(&gpu, cmd, test_image,
									 GPU_IMAGE_STATE_SHADER_READ);
			gpu.temp_set_image_desc(&gpu, full_view);
		}

		{ // [TODO]: temp
			gpu_image_view v = {0};
			ASSERT(gpu.get_image_full_view(&gpu, &v, frame.framebuffer));
			ASSERT(v.internal && v.internal == frame.framebuffer_view.internal);
		}

		gpu_color_target color_target = {
			.clear_color = (gpu_color){.floats = {.3f, .7f, .2f, 1.f}},
			.load_op = GPU_LOAD_OP_CLEAR,
			.store_op = GPU_STORE_OP_STORE,
			.target = frame.framebuffer,
		};
		gpu_render_state s = {
			.color_target_count = 1,
			.color_targets = {color_target},
		};
		gpu.cmd_transition_image(&gpu, cmd, frame.framebuffer,
								 GPU_IMAGE_STATE_RENDER_TARGET);
		gpu.cmd_reset_viewport(&gpu, cmd);
		gpu.cmd_reset_scissor(&gpu, cmd);
		gpu.cmd_begin_rendering(&gpu, cmd, &s);
		gpu.cmd_bind_pipeline(&gpu, cmd, pipeline);
		gpu.cmd_draw(&gpu, cmd, 6, 1, 0, 0);
		gpu.cmd_end_rendering(&gpu, cmd);
		gpu.cmd_transition_for_present(&gpu, cmd, frame.framebuffer);
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

	gpu.wait_idle(&gpu);
	gpu.destroy_image(&gpu, test_image);
	gpu.destroy_pipeline(&gpu, pipeline);

	gpu.destroy(&gpu);
	platform_destroy_window(win);
	platform_destroy();

	return 0;
}
