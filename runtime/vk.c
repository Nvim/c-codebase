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

typedef struct {
	float pos[4];
} tmp_vertex;

static tmp_vertex const bot_left = {-.75f, -.75f, 0.f, 0.f};
static tmp_vertex const bot_right = {.75f, -.75f, 0.f, 0.f};
static tmp_vertex const top_right = {.75f, .75f, 0.f, 0.f};
static tmp_vertex const top_left = {-.75f, .75f, 0.f, 0.f};

static tmp_vertex vertex_data[] = {bot_left, top_right, bot_right,
								   bot_left, top_left,	top_right};

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

	gpu_buffer_ci vbuf_ci = {0};
	{
		vbuf_ci.size = sizeof(vertex_data);
		vbuf_ci.memory_preference = GPU_BUFFER_MEMORY_GPU;
		vbuf_ci.usage = GPU_BUFFER_USAGE_STORAGE;
	}
	gpu_buffer vertex_buffer = gpu.create_buffer(&gpu, &vbuf_ci);
	gpu_buffer_device_address_result ret =
		gpu.buffer_get_device_address(&gpu, vertex_buffer);
	ASSERT(ret.ok);
	gpu_device_address vbuf_addr = ret.address;

	gpu_buffer_ci staging_ci = {0};
	{
		staging_ci.size = 4 * MEMORY_KIB;
		staging_ci.usage = GPU_BUFFER_USAGE_STAGING_UP;
	}
	gpu_buffer staging_buffer = gpu.create_buffer(&gpu, &staging_ci);

	gpu_data_payload vbuf_payload = {0};
	{
		vbuf_payload.data_format = GPU_DATA_SPAN;
		vbuf_payload.data.span.data = vertex_data;
		vbuf_payload.data.span.size = sizeof(vertex_data);
	}
	gpu.copy_to_staging_buffer(&gpu, staging_buffer, vbuf_payload);

	{ // Upload image and vertices and wait
		gpu_command_buffer cmd = gpu.create_command_buffer(&gpu);
		ASSERT(check_handle(cmd.handle));
		gpu.cmd_transition_image(&gpu, cmd, test_image,
								 GPU_IMAGE_STATE_TRANSFER_DST);
		ASSERT(gpu.temp_copy_to_image(&gpu, cmd, test_image, test_image_data));
		gpu.cmd_transition_image(&gpu, cmd, test_image,
								 GPU_IMAGE_STATE_SHADER_READ);

		gpu_buffer_copy_info buf_copy = {0};
		{
			buf_copy.size = sizeof(vertex_data);
			buf_copy.src = staging_buffer;
			buf_copy.dst = vertex_buffer;
		}
		ASSERT(gpu.cmd_copy_to_buffer(&gpu, cmd, &buf_copy));

		gpu_fence fence = gpu.create_fence(&gpu, false);
		ASSERT(gpu.submit_command_buffer_fence(&gpu, cmd, fence));
		ASSERT(gpu.wait_fence(&gpu, fence, sec_to_ns(1)));

		gpu.destroy_command_buffer(&gpu, cmd);
		gpu.destroy_fence(&gpu, fence);
	}

	{ // [TODO]: temp
		gpu_sampler_ci ci = {0};
		gpu_sampler sampler = gpu.create_sampler(&gpu, &ci);
		gpu.destroy_sampler(&gpu, sampler);
	}

	{ // [TODO]: temp buffer tests
		gpu_buffer_ci ci = {0};
		{
			ci.size = 4 * MEMORY_KIB;
			ci.usage = GPU_BUFFER_USAGE_STAGING_UP;
			ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
		}
		gpu_buffer buffer = gpu.create_buffer(&gpu, &ci);
		gpu_buffer_map_result res = gpu.buffer_get_mapped(&gpu, buffer);
		ASSERT(res.ok && res.ptr);
		gpu.destroy_buffer(&gpu, buffer);

		/* ================================================================== */
		{
			ci.usage = GPU_BUFFER_USAGE_STORAGE;
			ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
		}
		buffer = gpu.create_buffer(&gpu, &ci);
		res = gpu.buffer_get_mapped(&gpu, buffer);
		ASSERT(res.ok && res.ptr);
		gpu.destroy_buffer(&gpu, buffer);

		/* ================================================================== */
		{
			ci.usage = GPU_BUFFER_USAGE_STORAGE;
			ci.memory_preference = GPU_BUFFER_MEMORY_GPU;
		}
		buffer = gpu.create_buffer(&gpu, &ci);
		res = gpu.buffer_get_mapped(&gpu, buffer);
		ASSERT(!res.ok && !res.ptr);
		gpu.destroy_buffer(&gpu, buffer);
	}

	uint32_t frame_count = 0;
	while (!_quit) {
		frame_count++;

		platform_poll();
		if (platform_window_should_close(win)) { break; }

		gpu_frame_result frame_res = gpu.begin_frame(&gpu);
		if(frame_res.status == FRAME_ERROR) {
			_quit = true;
			break;
		}
		if(frame_res.status == FRAME_SKIP) {
			gpu.end_frame(&gpu, &frame_res.frame);
			LOG_INFO("SKIPPING FRAME");
			continue;
		}
		gpu_frame frame = frame_res.frame;
		gpu_command_buffer cmd = frame.frame_cmd;

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
		LOG_WARN("FRAME %u", frame_count);

		if (frame_count == 1) {
			// Set image binding on 1st frame only
			gpu.temp_set_image_desc(&gpu, full_view);
		}
			.color_target_count = 1,
			.color_targets = {color_target},
		};
		gpu.cmd_transition_image(&gpu, cmd, frame.framebuffer,
								 GPU_IMAGE_STATE_RENDER_TARGET);
		gpu.cmd_reset_viewport(&gpu, cmd);
		gpu.cmd_reset_scissor(&gpu, cmd);
		gpu.cmd_begin_rendering(&gpu, cmd, &s);
		gpu.cmd_bind_pipeline(
			&gpu, cmd, pipeline,
			&(gpu_per_frame_data){.vertex_buffer = vbuf_addr});
		gpu.cmd_draw(&gpu, cmd, 6, 1, 0, 0);
		gpu.cmd_end_rendering(&gpu, cmd);
		ASSERT(gpu.end_frame(&gpu, &frame));
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
	gpu.destroy_buffer(&gpu, staging_buffer);
	gpu.destroy_buffer(&gpu, vertex_buffer);
	gpu.destroy_pipeline(&gpu, pipeline);

	gpu.destroy(&gpu);
	platform_destroy_window(win);
	platform_destroy();

	return 0;
}
