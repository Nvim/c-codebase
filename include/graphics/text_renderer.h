#include "graphics.h"

#define TEXT_MAX_SCALE 0.125f
#define TEXT_MIN_SCALE 5.f

typedef struct text_renderer_t text_renderer_t;

LIB_EXPORT text_renderer_t* text_renderer_init(gpu_api* gpu, arena_t* arena);

LIB_EXPORT void text_renderer_destroy(text_renderer_t* renderer);

typedef struct font_atlas_info {
	gpu_image_extent glyph_size;
	uint8_t first_char; // First char in ASCII table
	uint8_t last_char;	// Last char in ASCII table
	uint8_t pad_x;		// Left/Right padding around char
	uint8_t pad_y;		// Top/Bottom padding around char
} font_atlas_info;

// [TODO]: asset system -> asset handle version
LIB_EXPORT bool text_renderer_load_atlas_from_file(text_renderer_t* renderer,
												   arena_t* scratch, path path,
												   font_atlas_info atlas_info);


/**
 * @brief Add text to draw for current frame
 *
 * @param renderer
 * @param text String to write. Chars must be in the loaded atlas' range
 * @param x X coordinate from 0% to 100%.
 * @param y Y coordinate from 0% to 100%.
 * @param scale Text scale. Must be between TEXT_MIN_SCALE and TEXT_MAX_SCALE
 * @return True on success, false on failure.
 */
LIB_EXPORT bool text_renderer_add_text(text_renderer_t* renderer,
												string text, float x, float y,
												float scale);

/**
 * [NOTE] Post-processing step on top of framebuffer.
 * This is more convenient, but I should support drawing on any command
 * buffer+image view
 */
LIB_EXPORT bool text_renderer_draw(text_renderer_t* renderer,
								   gpu_command_buffer cmd,
								   gpu_image_view target);
