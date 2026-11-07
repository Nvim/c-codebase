#ifndef CAMERA_2D_H
#define CAMERA_2D_H

#include "renderer.h"

camera_2d_handle camera_2d_create(renderer* renderer, camera_2d_props props);

void camera_2d_update(renderer* renderer, camera_2d_handle handle,
					  camera_2d_props props);

camera_2d_props camera_2d_get_props(renderer* renderer,
									camera_2d_handle handle);

// mat4 camera_2d_get_viewproj(camera_2d* camera);

#endif // !CAMERA_2D_H
