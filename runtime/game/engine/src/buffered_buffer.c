#include "buffered_buffer.h"

#include <string.h>

bool buffered_buffer_init(buffered_buffer* out, gpu_api* gpu, gpu_buffer_ci* ci,
						  uint8_t count, bool get_addresses) {
	if (!count || count > GPU_MAX_BUFFERING) { return false; }
	memset(out, 0, sizeof(*out));

	bool good = true;
	uint8_t i = 0;
	for (; i < count; i++) {
		gpu_buffer b = gpu->create_buffer(gpu, ci);
		if (!check_handle(b.handle)) {
			good = false;
			break;
		}

		if (get_addresses) {
			gpu_buffer_device_address_result ret =
				gpu->buffer_get_device_address(gpu, b);
			if (!ret.ok) {
				good = false;
				break;
			}
			out->addresses[i] = ret.address;
		}

		if (ci->map) {
			gpu_buffer_map_result mret = gpu->buffer_get_mapped(gpu, b);
			if (!mret.ok) {
				good = false;
				break;
			}
			out->maps[i] = mret.ptr;
		}

		out->buffers[i] = b;
	}

	if (!good) {
		for (uint8_t j = 0; j < i; j++) {
			gpu->destroy_buffer(gpu, out->buffers[i]);
		}
		memset(out, 0, sizeof(*out));
		return false;
	}

	out->count = count;
	return true;
}

void buffered_buffer_advance(buffered_buffer* bufs) {
	bufs->current = (bufs->current + 1) % bufs->count;
}

gpu_buffer buffered_buffer_get(buffered_buffer* bufs) {
	return bufs->buffers[bufs->current];
}

gpu_device_address buffered_buffer_get_addr(buffered_buffer* bufs) {
	return bufs->addresses[bufs->current];
}

void* buffered_buffer_get_mapped(buffered_buffer* bufs) {
	return bufs->maps[bufs->current];
}

void buffered_buffer_destroy(buffered_buffer* bufs, gpu_api* gpu) {
	for (uint8_t i = 0; i < bufs->count; ++i) {
		gpu->destroy_buffer(gpu, bufs->buffers[i]);
	}
}
