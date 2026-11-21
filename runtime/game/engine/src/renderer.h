#ifndef RENDERER_H
#define RENDERER_H

#include "buffered_buffer.h"
#include "transfer_queue.h"
#include <engine/render_interface.h>

#include <core/math.h>
#include <graphics/graphics.h>

POOL_DEFINE_TYPE(camera_2d_props, camera_2d_pool)

typedef struct camera_2d_matrices {
	mat4 view_from_world; // view
	mat4 clip_from_view;  // projection
} camera_2d_matrices;

typedef struct renderer_uniform_data {
	// gpu_device_address camera_matrices;
	mat4 view_from_world;
	mat4 clip_from_view;
	gpu_device_address draw_items;
	float time;
} renderer_uniform_data;

typedef struct render_pass {
	uint32_t index;
	string name;
	renderer_uniform_data uniform;
	uint32_t first_instance;
	uint32_t draw_count;
	// pipeline, render target, viewport & scissor things?
} render_pass;

typedef struct internal_draw_item {
	vec2 pos;
	vec2 size;
	vec2 uv0;
	vec2 uv1;
	vec4 color;
	int texture_index;
	int sampler_index;
} internal_draw_item;

ARRAY_DEFINE_TYPE(internal_draw_item, internal_draw_item_array)

typedef struct font_atlas_info {
	vec2 glyph_size;
	uint8_t first_char;
	uint8_t last_char;
	uint8_t pad_x;
	uint8_t pad_y;
} font_atlas_info;

typedef struct renderer {
	arena_t* arena;		  // permanent allocs
	arena_t* frame_arena; // frame allocs

	gpu_api* gpu;
	gpu_pipeline pipeline;
	transfer_queue* transfer_queue;
	gpu_buffer quad_index_buffer;
	buffered_buffer draw_buffer;	// gpu_indirect_draw_command
	buffered_buffer gpu_draw_items; // internal_draw_item
	internal_draw_item_array cpu_draw_items;
	buffered_buffer uniform_buffer; // renderer_uniform_data

	// TODO: safe array with bounds checking
	camera_2d_matrices camera_matrices[RENDERER_MAX_CAMERAS];
	camera_2d_pool cameras;
	camera_2d_handle default_camera_handle;
	gpu_image_extent logical_resolution;
	float scale;

	// Text rendering things:
	gpu_image font_atlas;
	gpu_image_view font_atlas_texture;
	gpu_image_extent font_atlas_size;
	gpu_sampler font_sampler;
	uint32_t font_atlas_binding;
	uint32_t font_sampler_binding;
	font_atlas_info font_atlas_info;

	/**
	 * If false, device doesn't have (enough) unified memory and we must upload
	 * data with staging buffers and transfer commands.
	 * If true, use CPU-mapped unified memory for quick uploads with memcpy.
	 */
	bool use_uma;

	gpu_frame* cur_frame;
	uint32_t draw_count; // Draws for current frame. Reset every frame
	render_pass cur_pass;
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

bool renderer_draw(renderer* renderer, renderer_draw_item_array draw_items,
				   uint64_t draw_count);

bool renderer_draw_ui(renderer* renderer, renderer_ui_item_array items,
					  uint64_t draw_count);

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

#endif // !RENDERER_H
