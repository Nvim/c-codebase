#ifndef TRANSFER_QUEUE_H
#define TRANSFER_QUEUE_H

#include <containers/array.h>
#include <graphics/graphics.h>

typedef struct transfer_queue transfer_queue;

/**
 * @brief Initialize transfer queue. Will create staging buffer and tasks array.
 *
 * @param arena arena on which queue and will be allocated
 * @param gpu gpu for staging buffer creation
 * @param buffer_size size of the staging buffer in bytes
 * @param max_uploads_in_flight max amount of concurrent pending tasks
 * @return the new queue
 */
transfer_queue* transfer_queue_init(arena_t* arena, gpu_api* gpu,
									uint64_t buffer_size,
									uint32_t max_uploads_in_flight);

void transfer_queue_destroy(transfer_queue* transfer_queue);

/**
 * @brief Enqueue an upload to buffer.
 *
 * @param q the queue
 * @param payload data to upload. `dst_offset` is interpreted as the offset to
 * copy in dest
 * @param dest destination buffer
 * @return true on success, false otherwise
 */
bool transfer_queue_upload_buffer(transfer_queue* q, gpu_data_payload payload,
								  gpu_buffer dest,
								  gpu_buffer_access buf_access);

bool transfer_queue_upload_image(transfer_queue* queue,
								 gpu_image_data_payload payload);

/**
 * @brief Poll state of all pending transfers, reset finished command buffers,
 * and reclaim staging buffer chunks. Doesn't stall CPU, should be called
 * frequently.
 */
bool transfer_queue_tick(transfer_queue* queue);

/**
 * @brief Batch all pending transfers to GPU.
 * Transfer commands are all recorded and submitted on a dedicated command
 * buffer.
 * [NOTE]: For now, command buffer is guaranteed to be on the same
 * unified queue as graphics, so sync is managed with barriers only.
 * Once GPU API exposes dedicated queues, we'll need semaphore from this.
 */
bool transfer_queue_batch_process(transfer_queue* queue);

/**
 * @brief Batch all pending transfers to GPU and wait for completion (will stall
 * CPU until all uploads are done).
 * Transfer commands are all recorded on a dedicated command buffer, and the
 * whole command buffer submission is waited upon.
 */
bool transfer_queue_batch_process_wait(transfer_queue* queue);

#endif // !TRANSFER_QUEUE_H
