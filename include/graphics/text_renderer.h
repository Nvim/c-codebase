#include "graphics.h"

typedef struct text_renderer_t text_renderer_t;

LIB_EXPORT text_renderer_t* text_renderer_init(gpu_api* gpu, arena_t* arena);

LIB_EXPORT void text_renderer_destroy(text_renderer_t* renderer);

LIB_EXPORT void text_renderer_set_scale(text_renderer_t* renderer, float scale);

typedef struct font_atlas_info {
	gpu_image_extent glyph_size;
	uint32_t first_char; // First char in ASCII table
	uint32_t last_char;	 // Last char in ASCII table
} font_atlas_info;

// [TODO]: asset system -> asset handle version
LIB_EXPORT bool text_renderer_load_atlas_from_file(text_renderer_t* renderer,
												   arena_t* scratch, path path,
												   font_atlas_info atlas_info);

LIB_EXPORT bool text_renderer_add_text(text_renderer_t* renderer, string text,
									   uint32_t x, uint32_t y);

/**
 * [NOTE] Post-processing step on top of framebuffer.
 * This is more convenient, but I should support drawing on any command
 * buffer+image view
 */
LIB_EXPORT bool text_renderer_draw(text_renderer_t* renderer, gpu_frame cmd);
