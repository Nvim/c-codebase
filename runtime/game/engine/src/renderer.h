#ifndef RENDERER_H
#define RENDERER_H

#include <core/math.h>
#include <graphics/graphics.h>

typedef struct renderer {
	arena_t* arena;		  // permanent allocs
	arena_t* frame_arena; // frame allocs

	gpu_api* gpu;
	gpu_pipeline pipeline;
	gpu_buffer staging_buffer; // [TODO]: should i use a single one ?
	gpu_buffer draw_buffer[3]; // [TODO]: get buffered_buffer from text renderer
} renderer;

typedef struct renderer_camera {
	mat4 mat_model;
	mat4 mat_view;
	mat4 mat_proj;
} renderer_camera;

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

bool renderer_init(arena_t* arena, gpu_api_config* config);

bool renderer_shutdown(renderer* renderer);

// Frame flow
// begin gpu frame
bool renderer_begin_frame(renderer* renderer);

// begin renderpass (cmd_begin_rendering);
bool renderer_begin_pass(renderer* renderer);

// end renderpass (cmd_end_rendering);
bool renderer_end_pass(renderer* renderer);

// submit & present gpu frame
bool renderer_end_frame(renderer* renderer);

// Draw Commands

// Resource creation
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
