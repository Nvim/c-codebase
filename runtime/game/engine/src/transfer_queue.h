#ifndef TRANSFER_QUEUE_H
#define TRANSFER_QUEUE_H

#include <containers/array.h>
#include <graphics/graphics.h>

typedef struct transfer_upload_task {
	gpu_fence fence;
	bool done; // maybe redundant
	uint64_t start_pos;
	uint64_t size;
} transfer_upload_task;

ARRAY_DEFINE_TYPE(transfer_upload_task, transfer_upload_task_array)

typedef struct transfer_queue {
	gpu_buffer staging_buf;
	uint64_t current_offset;
	transfer_upload_task_array uploads;
} transfer_queue;

/**
 * @brief Initialize transfer queue. Will create staging buffer and tasks array.
 *
 * @param out resulting transfer queue.
 * @param arena arena on which tasks array will be allocated
 * @param gpu gpu for staging buffer creation
 * @param buffer_size size of the staging buffer in bytes
 * @param max_uploads_in_flight max amount of concurrent pending tasks
 * @return false on failure, true on success
 */
bool transfer_queue_init(transfer_queue* out, arena_t* arena, gpu_api* gpu,
						 uint64_t buffer_size, uint64_t max_uploads_in_flight);

/**
 * @brief Batch all pending transfers to GPU and wait for completion (will stall
 * CPU until all uploads are done).
 * Transfer commands are all recorded on a dedicated command buffer, and the
 * whole command buffer submission is waited upon.
 */
bool transfer_queue_batch_process(transfer_queue* queue, gpu_api* gpu);

#endif // !TRANSFER_QUEUE_H
