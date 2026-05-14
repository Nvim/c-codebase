#ifndef GRAPHICS_INTERNAL_H
#define GRAPHICS_INTERNAL_H

#include <graphics/graphics.h>

bool gpu_validate_image_ci(gpu_image_ci* ci);

bool gpu_validate_buffer_ci(gpu_buffer_ci* ci);

static inline bool is_2d_image(gpu_image_type t) {
	// Always true for now
	return (t == GPU_IMAGE_TYPE_2D || t == GPU_IMAGE_TYPE_2D_ARRAY ||
			t == GPU_IMAGE_TYPE_CUBE || t == GPU_IMAGE_TYPE_CUBE_ARRAY);
}

#endif // !GRAPHICS_INTERNAL_H
