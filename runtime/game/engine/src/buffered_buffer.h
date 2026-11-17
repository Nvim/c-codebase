#ifndef BUFFERED_BUFFER_H
#define BUFFERED_BUFFER_H

#include <graphics/graphics.h>

typedef struct buffered_buffer {
	gpu_buffer buffers[GPU_MAX_BUFFERING];
	gpu_device_address addresses[GPU_MAX_BUFFERING];
	void* maps[GPU_MAX_BUFFERING];
	uint8_t count;
	uint8_t current;
} buffered_buffer;

bool buffered_buffer_init(buffered_buffer* out, gpu_api* gpu, gpu_buffer_ci* ci,
						  uint8_t count, bool get_addresses);

void buffered_buffer_advance(buffered_buffer* bufs);

gpu_buffer buffered_buffer_get(buffered_buffer* bufs);

gpu_device_address buffered_buffer_get_addr(buffered_buffer* bufs);

void* buffered_buffer_get_mapped(buffered_buffer* bufs);

void buffered_buffer_destroy(buffered_buffer* bufs, gpu_api* gpu);

#endif // !BUFFERED_BUFFER_H
