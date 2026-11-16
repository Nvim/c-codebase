#ifndef RENDER_INTERFACCE_H
#define RENDER_INTERFACCE_H

#include <containers/array.h>
#include <core/math.h>
#include <graphics/graphics.h>

#define RENDERER_MAX_DRAWS 1024
#define RENDERER_MAX_CAMERAS 8
#define RENDERER_MAX_PASSES 8

typedef struct camera_2d_props {
	vec2 position;				 // position in world space
	gpu_image_extent dimensions; // extents in world space
	float zoom;
} camera_2d_props;

typedef struct {
	pool_handle handle;
} camera_2d_handle;

// Colored quad
typedef struct renderer_draw_item {
	gpu_color color;
	vec2 position;
	vec2 scale;
} renderer_draw_item;

ARRAY_DEFINE_TYPE(renderer_draw_item, renderer_draw_item_array)

typedef struct renderer_packet {
	camera_2d_handle camera;
	renderer_draw_item_array draw_items;
	size_t draw_count;
} renderer_packet;

#endif // !RENDER_INTERFACCE_H
