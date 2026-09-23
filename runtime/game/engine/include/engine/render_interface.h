#ifndef RENDER_INTERFACCE_H
#define RENDER_INTERFACCE_H

#include <core/math.h>
#include <graphics/graphics.h>
#include <containers/array.h>

#define MAX_DRAW_ITEMS 1024

typedef struct renderer_camera renderer_camera;

// Colored quad
typedef struct renderer_draw_item {
	gpu_color color;
	vec2 position;
	float scale;
} renderer_draw_item;

ARRAY_DEFINE_TYPE(renderer_draw_item, renderer_draw_item_array)

typedef struct renderer_packet {
	// renderer_camera* camera;
	renderer_draw_item_array draw_items;
	size_t draw_count;
} renderer_packet;

#endif // !RENDER_INTERFACCE_H
