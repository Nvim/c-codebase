#include <core/core.h>
#include <core/log.h>
#include <filesystem/filesystem.h>
#include <graphics/graphics.h>
#include <loaders/qoi_loader.h>
#include <platform/platform.h>

#include <limits.h>
#include <string.h>

static bool _quit = false;
UNUSED static double const TARGET_FRAMETIME_MS = (1 / 60.f) * 1000.f;

UNUSED static uint8_t const WHITE[4] = {0xff, 0xff, 0xff, 0xff};
UNUSED static uint8_t const RED[4] = {0xff, 0x0, 0x0, 0xff};
UNUSED static uint8_t const GREEN[4] = {0x0, 0xff, 0x0, 0xff};
UNUSED static uint8_t const BLUE[4] = {0x0, 0x0, 0xff, 0xff};

typedef struct {
	float pos[3];
	float _pad;
	float uv[2];
	float _pad2[2];
} tmp_vertex;

typedef struct {
	gpu_device_address vertex_buffer_addr;
	float time;
	uint32_t image_index;
	uint32_t sampler_index;
} uniform_data;

// clang-format off
static tmp_vertex const bot_left = {.pos = {-.75f, -.75f, 0.f}, .uv = {0.f, 0.f}};
static tmp_vertex const bot_right = {.pos = {.75f, -.75f, 0.f}, .uv = {1.f, 0.f}};
static tmp_vertex const top_right = {.pos = {.75f, .75f, 0.f}, .uv = {1.f, 1.f}};
static tmp_vertex const top_left = {.pos = {-.75f, .75f, 0.f}, .uv = {0.f, 1.f}};
// clang-format on

static tmp_vertex vertex_data[] = {bot_left, bot_right, top_right, top_left};
static uint32_t index_data[] = {0, 1, 2, 0, 2, 3};

static gpu_cmd_indirect_indexed_draw_command indirect_data = {
	.index_count = 6,
	.instance_count = 1,
	.first_index = 0,
	.vertex_offset = 0,
	.first_instance = 0,
};

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
		arena_ci.initial_size = 8 * MEMORY_MIB;
		arena_ci.max_size = 64 * MEMORY_MIB;
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
		gpu_ci.max_sampled_images = 32768;
		gpu_ci.max_samplers = 1024;
		gpu_ci.max_storage_images = 32768;
	}

	ASSERT(gpu_create_api(gpu_ci, &gpu));
	gpu.initialize(&gpu, win, arena, scratch.arena);
	ASSERT(gpu.is_init);

	arena_clear_scratch(scratch);

	gpu_shader vs = {0}, fs = {0};
	{ // Vertex shader
		path vs_path;
		ASSERT(path_create(arena, scratch.arena,
						   str("build/shaders/quad.slang.vert.spirv"),
						   &vs_path));
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
						   str("build/shaders/quad.slang.frag.spirv"),
						   &fs_path));
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

	image_read_result img_read = load_qoi(
		arena, scratch.arena, str("runtime/qoi_test_images/meep.qoi"), true);
	ASSERT(img_read.ok && img_read.channels == 4);
	arena_clear_scratch(scratch);

	gpu_image blitted;
	gpu_image test_image;
	{ // [TODO]: temp
		gpu_image_ci img_ci = {0};
		img_ci.dimensions = (gpu_image_extent){.width = img_read.width,
											   .height = img_read.height};
		img_ci.format = IMAGE_FORMAT_RGBA_UNORM;
		img_ci.type = GPU_IMAGE_TYPE_2D;
		img_ci.usage = GPU_IMAGE_USAGE_SAMPLED;
		img_ci.array_layers = 1;
		img_ci.mip_levels = 1;
		test_image = gpu.create_image(&gpu, &img_ci);
		ASSERT(check_handle(test_image.handle));
	}

	gpu_data_payload const test_image_data = {
		.data_format = GPU_DATA_ARRAY,
		.data.array = img_read.bytes,
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
	gpu_device_address vertex_addr = ret.address;

	gpu_buffer_ci ibuf_ci = {0};
	{
		ibuf_ci.size = sizeof(index_data);
		ibuf_ci.usage = GPU_BUFFER_USAGE_INDEX;
	}
	gpu_buffer index_buffer = gpu.create_buffer(&gpu, &ibuf_ci);

	gpu_buffer_ci indirect_buf_ci = {0};
	{
		indirect_buf_ci.size = sizeof(indirect_data);
		indirect_buf_ci.usage = GPU_BUFFER_USAGE_INDIRECT;
		indirect_buf_ci.memory_preference = GPU_BUFFER_MEMORY_GPU;
	}
	gpu_buffer indirect_buffer = gpu.create_buffer(&gpu, &indirect_buf_ci);

	gpu_buffer_ci staging_ci = {0};
	{
		staging_ci.size = 4 * MEMORY_MIB;
		staging_ci.usage = GPU_BUFFER_USAGE_STAGING_UP;
	}
	gpu_buffer staging_buffer = gpu.create_buffer(&gpu, &staging_ci);

	gpu_buffer_ci uniform_buffer_ci = {0};
	{
		uniform_buffer_ci.size = sizeof(uniform_data);
		uniform_buffer_ci.usage = GPU_BUFFER_USAGE_STORAGE;
		uniform_buffer_ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
	}
	gpu_buffer uniform_buffer = gpu.create_buffer(&gpu, &uniform_buffer_ci);
	ret = gpu.buffer_get_device_address(&gpu, uniform_buffer);
	ASSERT(ret.ok);
	gpu_device_address uniform_addr = ret.address;
	gpu_buffer_map_result mret = gpu.buffer_get_mapped(&gpu, uniform_buffer);
	ASSERT(mret.ok);
	uniform_data* uniform = mret.ptr;
	{
		uniform->time = 0.f;
		uniform->vertex_buffer_addr = vertex_addr;
	};

	size_t offset = 0;
	gpu_image_data_payload img_payload = {
		.data = test_image_data,
		.height = img_read.width,
		.width = img_read.height,
	};
	gpu.copy_to_staging_buffer(&gpu, staging_buffer, img_payload.data);
	offset += img_read.channels * u8_array_size(img_read.bytes);

	gpu_data_payload vbuf_payload = {0};
	{
		vbuf_payload.data_format = GPU_DATA_SPAN;
		vbuf_payload.data.span.data = vertex_data;
		vbuf_payload.data.span.size = sizeof(vertex_data);
		vbuf_payload.offset = offset;
	}
	gpu.copy_to_staging_buffer(&gpu, staging_buffer, vbuf_payload);
	offset += sizeof(vertex_data);

	gpu_data_payload ibuf_payload = {0};
	{
		ibuf_payload.data_format = GPU_DATA_SPAN;
		ibuf_payload.data.span.data = index_data;
		ibuf_payload.data.span.size = sizeof(index_data);
		ibuf_payload.offset = offset;
	}
	gpu.copy_to_staging_buffer(&gpu, staging_buffer, ibuf_payload);
	offset += sizeof(index_data);

	gpu_data_payload indirect_payload = {0};
	{
		indirect_payload.data_format = GPU_DATA_SPAN;
		indirect_payload.data.span.data = &indirect_data;
		indirect_payload.data.span.size = sizeof(indirect_data);
		indirect_payload.offset = offset;
	}
	gpu.copy_to_staging_buffer(&gpu, staging_buffer, indirect_payload);

	{ // Upload image and vertices and wait
		gpu_command_buffer cmd = gpu.create_command_buffer(&gpu);
		ASSERT(check_handle(cmd.handle));
		gpu.cmd_transition_image(&gpu, cmd, test_image,
								 GPU_IMAGE_STATE_TRANSFER_DST);

		gpu_image_region_array regions =
			gpu_image_region_array_create(scratch.arena, 1);
		ASSERT(regions.raw);
		gpu_image_region* reg = gpu_image_region_array_begin(regions);
		{
			reg->layer = 0;
			reg->level = 0;
		}

		offset = 0;
		gpu_image_copy_info img_copy_info = {
			.dst = test_image,
			.src = staging_buffer,
			.src_offset = offset,
			.regions = regions,
		};
		ASSERT(gpu.cmd_copy_to_image(&gpu, cmd, &img_copy_info));
		offset += img_read.channels * u8_array_size(img_read.bytes);

		gpu_buffer_copy_info buf_copy = {0};
		{
			buf_copy.size = sizeof(vertex_data);
			buf_copy.src = staging_buffer;
			buf_copy.src_offset = offset;
			buf_copy.dst = vertex_buffer;
		}
		ASSERT(gpu.cmd_copy_to_buffer(&gpu, cmd, &buf_copy));
		offset += buf_copy.size;

		{
			buf_copy.size = sizeof(index_data);
			buf_copy.src = staging_buffer;
			buf_copy.src_offset = offset;
			buf_copy.dst = index_buffer;
		}
		ASSERT(gpu.cmd_copy_to_buffer(&gpu, cmd, &buf_copy));
		offset += buf_copy.size;

		{
			buf_copy.size = sizeof(indirect_data);
			buf_copy.src = staging_buffer;
			buf_copy.src_offset = offset;
			buf_copy.dst = indirect_buffer;
		}
		ASSERT(gpu.cmd_copy_to_buffer(&gpu, cmd, &buf_copy));

		{ // TEMP: test image blit
			gpu_image_ci img_ci = {0};
			img_ci.dimensions = (gpu_image_extent){.width = img_read.width,
												   .height = img_read.height};
			img_ci.format = IMAGE_FORMAT_RGBA_UNORM;
			img_ci.type = GPU_IMAGE_TYPE_2D;
			img_ci.usage = GPU_IMAGE_USAGE_SAMPLED;
			img_ci.array_layers = 1;
			img_ci.mip_levels = 1;

			blitted = gpu.create_image(&gpu, &img_ci);
			ASSERT(check_handle(blitted.handle));

			gpu_image_region region = {.layer = 0, .level = 0};

			gpu_image_blit_info blit_ci = {0};
			{
				blit_ci.src_region = region;
				blit_ci.dst_region = region;
				blit_ci.filter = GPU_SAMPLER_FILTER_LINEAR;
				blit_ci.src = test_image;
				blit_ci.dst = blitted;
			}

			ASSERT(gpu.cmd_blit_to_image(&gpu, cmd, &blit_ci));
			// It works in renderdoc trust me bro
		}

		gpu.cmd_transition_image(&gpu, cmd, test_image,
								 GPU_IMAGE_STATE_SHADER_READ);

		gpu_fence fence = gpu.create_fence(&gpu, false);
		ASSERT(gpu.submit_command_buffer_fence(&gpu, cmd, fence));
		ASSERT(gpu.wait_fence(&gpu, fence, sec_to_ns(1)));

		gpu.destroy_command_buffer(&gpu, cmd);
		gpu.destroy_fence(&gpu, fence);
	}

	gpu_sampler sampler;
	gpu_sampler_ci ci = {0};
	sampler = gpu.create_sampler(&gpu, &ci, &uniform->sampler_index);

	// Create texture so image gets a shader binding
	gpu_image_view_ci view_ci = {0};
	{
		view_ci.image = test_image;
		view_ci.type = GPU_IMAGE_VIEW_TEXTURE;
		view_ci.base_layer = 0;
		view_ci.base_level = 0;
		view_ci.layer_count = GPU_COUNT_REMAINING;
		view_ci.level_count = GPU_COUNT_REMAINING;
	}
	gpu_image_view texture_view = gpu.create_image_view(&gpu, &view_ci);
	ASSERT(check_handle(texture_view.handle));
	ASSERT(gpu.get_image_view_shader_index(&gpu, texture_view,
										   &uniform->image_index));

	double t_prev_s = platform_get_time_s();
	double delta_ms = 0.0f;

	while (!_quit) {

		platform_poll();
		if (platform_window_should_close(win)) { break; }

		gpu_frame_result frame_res = gpu.begin_frame(&gpu);
		if (frame_res.status == FRAME_ERROR) {
			_quit = true;
			break;
		}
		if (frame_res.status == FRAME_SKIP) {
			gpu.end_frame(&gpu, &frame_res.frame);
			LOG_INFO("SKIPPING FRAME");
			continue;
		}
		gpu_frame frame = frame_res.frame;
		gpu_command_buffer cmd = frame.frame_cmd;

		uniform->time = t_prev_s;

		gpu_color_target color_target = {
			.clear_color = (gpu_color){.floats = {.3f, .7f, .2f, 1.f}},
			.load_op = GPU_LOAD_OP_CLEAR,
			.store_op = GPU_STORE_OP_STORE,
			.target = frame.framebuffer_view,
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
		gpu.cmd_bind_pipeline(&gpu, cmd, pipeline,
							  &(gpu_per_frame_data){
								  .frame_buffer_address = uniform_addr,
							  });
		ASSERT(gpu.cmd_bind_index_buffer(&gpu, cmd, index_buffer));
		gpu.cmd_draw_indirect_indexed(
			&gpu, cmd, indirect_buffer, 0, 1,
			sizeof(gpu_cmd_indirect_indexed_draw_command));
		gpu.cmd_end_rendering(&gpu, cmd);
		ASSERT(gpu.end_frame(&gpu, &frame));
		double t_now_s = platform_get_time_s();
		delta_ms = (t_now_s - t_prev_s) * 1000.f;
		if (delta_ms < TARGET_FRAMETIME_MS) {
			platform_sleep(TARGET_FRAMETIME_MS - delta_ms);
			// LOG_DEBUG("frame took %f ms. slept for %f ms", delta_ms,
			// 		  TARGET_FRAMETIME_MS - delta_ms);
			// LOG_DEBUG("==============================================");
		}
		t_now_s = platform_get_time_s();
		t_prev_s = t_now_s;
	}

	gpu.wait_idle(&gpu);
	gpu.destroy_image_view(&gpu, texture_view);
	gpu.destroy_image(&gpu, test_image);
	gpu.destroy_image(&gpu, blitted);
	gpu.destroy_buffer(&gpu, uniform_buffer);
	gpu.destroy_buffer(&gpu, staging_buffer);
	gpu.destroy_buffer(&gpu, vertex_buffer);
	gpu.destroy_buffer(&gpu, index_buffer);
	gpu.destroy_buffer(&gpu, indirect_buffer);
	gpu.destroy_sampler(&gpu, sampler);
	gpu.destroy_pipeline(&gpu, pipeline);

	gpu.destroy(&gpu);
	platform_destroy_window(win);
	platform_destroy();

	return 0;
}
