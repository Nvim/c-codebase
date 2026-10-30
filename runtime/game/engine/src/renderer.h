#ifndef RENDERER_H
#define RENDERER_H

#include "transfer_queue.h"
#include <engine/render_interface.h>

#include <core/math.h>
#include <graphics/graphics.h>

typedef struct buffered_buffer {
	gpu_buffer buffers[GPU_MAX_BUFFERING];
	gpu_device_address addresses[GPU_MAX_BUFFERING];
	void* maps[GPU_MAX_BUFFERING];
	uint8_t count;
	uint8_t current;
} buffered_buffer;

typedef struct camera_2d_matrices {
	mat4 view_from_world; // view
	mat4 clip_from_view;  // projection
} camera_2d_matrices;

typedef struct camera_2d {
	camera_2d_matrices matrices; // shader accesses this
	camera_2d_props props;		 // user touches this
} camera_2d;

#define RENDERER_MAX_CAMERAS 16
POOL_DEFINE_TYPE(camera_2d, camera_2d_pool)

typedef struct renderer_uniform_data {
	gpu_device_address draw_items;
	gpu_device_address camera_matrices;
	uint32_t camera_index;
	float time;
} renderer_uniform_data;

typedef struct renderer {
	arena_t* arena;		  // permanent allocs
	arena_t* frame_arena; // frame allocs

	gpu_api* gpu;
	gpu_pipeline pipeline;
	transfer_queue* transfer_queue;
	gpu_buffer quad_index_buffer;
	buffered_buffer draw_buffer;		   // gpu_indirect_draw_command
	buffered_buffer quad_instances_buffer; // renderer_draw_item

	gpu_buffer cameras_buffer; // camera_2d
	gpu_device_address cameras_buffer_addr;

	gpu_buffer uniform_buffer;
	renderer_uniform_data* uniform_ptr; // CPU-mapped memory
	gpu_device_address uniform_addr;	// GPU memory

	/**
	 * If false, device doesn't have (enough) unified memory and we must upload
	 * data with staging buffers and transfer commands.
	 * If true, use CPU-mapped unified memory for quick uploads with memcpy.
	 */
	bool use_uma;

	gpu_frame* cur_frame;
	uint64_t draw_count; // Draws for current frame. Reset every frame

	camera_2d_pool cameras;
	gpu_image_extent logical_resolution;
	float scale;
} renderer;

typedef struct renderer_texture {
	gpu_image image;
	gpu_image_view image_view; // type is GPU_IMAGE_VIEW_TEXTURE

	/**
	 * [TODO]: Should i duplicate other info? (dimensions, layer & levels)
	 * If I don't, struct is only 8 bytes and i can pass it around easily. No
	 * data duplication, no state mismatch.
	 * But I'd have to query props at some point for sure. Question is how
	 * often.
	 * If it's too hard, I'd preferer renderer to have an internal cache
	 * hashtable instead of adding fields here.
	 */
} renderer_texture;

typedef struct renderer_texture_ci {
	path name; // [TODO]: name only, construct path from base dir.
	gpu_image_type type;
} renderer_texture_ci;

typedef struct renderer_buffer_ci {
	gpu_buffer_usage usage;
} renderer_buffer_ci;

/* *******************************[ RENDERER ]******************************* */
renderer* renderer_init(arena_t* arena, arena_t* frame_arena, arena_t* scratch,
						gpu_api* gpu, gpu_image_extent logical_resolution);

bool renderer_shutdown(renderer* renderer);

/* == Frame flow == */

// begin gpu frame
bool renderer_begin_frame(renderer* renderer);

// wether we should skip drawing this frame
bool renderer_should_draw(renderer* renderer);

// begin renderpass (cmd_begin_rendering);
bool renderer_begin_pass(renderer* renderer, camera_2d_handle camera);

// end renderpass (cmd_end_rendering);
bool renderer_end_pass(renderer* renderer);

// submit & present gpu frame
bool renderer_end_frame(renderer* renderer);

/* == Draw Commands == */
void renderer_draw_quad(renderer* renderer, vec2 position, vec3 color,
						vec2 scale);

/* == Resource creation == */
bool renderer_load_texture(renderer* renderer, renderer_texture* out,
						   renderer_texture_ci ci);

/**
 * I can live with raw gpu buffers for now i guess, just need to yoink
 * buffered_buffer.
 * Renderer should abstract upload process and staging vs map decision. So
 * don't use gpu_buffer_ci directly here.
 */
bool renderer_create_buffer(renderer* renderer, gpu_buffer* out,
							renderer_buffer_ci ci);

/* ********************************[ CAMERA ]******************************** */

camera_2d_handle camera_2d_create(renderer* renderer, camera_2d_props props);

void camera_2d_update(renderer* renderer, camera_2d_handle handle,
					  camera_2d_props props);

camera_2d_props camera_2d_get_props(renderer* renderer,
									camera_2d_handle handle);

mat4 camera_2d_get_viewproj(camera_2d* camera);

#endif // !RENDERER_H
