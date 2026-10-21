#include "transfer_queue.h"

bool transfer_queue_init(transfer_queue* out, arena_t* arena, gpu_api* gpu,
						 uint64_t buffer_size, uint64_t max_uploads_in_flight) {
	if (!out || !buffer_size || !max_uploads_in_flight) { return false; }

	gpu_buffer_ci ci = {0};
	{
		ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
		ci.size = buffer_size;
		ci.usage = GPU_BUFFER_USAGE_STAGING_UP;
	}

	gpu_buffer staging_buf = gpu->create_buffer(gpu, &ci);
	if (!check_handle(staging_buf.handle)) {
		LOG_ERR("couldn't create staging buffer");
		return false;
	}

	transfer_upload_task_array arr =
		transfer_upload_task_array_create(arena, max_uploads_in_flight);
	if (!arr.raw) {
		LOG_ERR("couldn't allocate upload tasks array");
		return false;
	}

	{
		out->current_offset = 0;
		out->staging_buf = staging_buf;
		out->uploads = arr;
	}

	return true;
}
