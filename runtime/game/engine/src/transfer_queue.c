#include "transfer_queue.h"

// [NOTE]: Each task allocates its own command buffer from
// `gpu_create_command_buffer`. I might want to keep a few ones in store and
// re-use them when done instead if performance is too bad.
typedef struct {
	// uint64_t id; ?
	gpu_command_buffer cmd;
	gpu_fence fence;
	uint64_t data_size;
	// all barriers
} transfer_task;

ARRAY_DEFINE_TYPE(transfer_task, transfer_task_array)

struct transfer_queue {
	gpu_api* gpu;

	transfer_task_array transfer_tasks;
	uint32_t current_task;	  // monotonic, wraps around
	uint32_t tasks_in_flight; // how many tasks are being processed

	gpu_buffer staging_buf;
	uint64_t total_usage_bytes;
	uint64_t max_single_task_size;
};

static void _log_queue(UNUSED transfer_queue* q) {
	LOG_DEBUG("\ttasks in flight: %u", q->tasks_in_flight);
	LOG_DEBUG("\tcurrent task: %u", q->current_task);
	LOG_DEBUG("\ttotal usage bytes: %lu", q->total_usage_bytes);
	LOG_DEBUG("\tmax single task size: %lu", q->max_single_task_size);
}

static bool _task_init(transfer_queue* q, transfer_task* t) {
	gpu_api* gpu = q->gpu;

	gpu_command_buffer cmd = gpu->create_command_buffer(gpu);
	gpu_fence fence = gpu->create_fence(gpu, false);

	if (!check_handle(cmd.handle) || !check_handle(fence.handle)) {
		return false;
	}

	t->cmd = cmd;
	t->fence = fence;
	t->data_size = 0;

	return true;
}

transfer_queue* transfer_queue_init(arena_t* arena, gpu_api* gpu,
									uint64_t buffer_size,
									uint32_t max_uploads_in_flight) {
	transfer_queue* ret = NULL;
	if (!buffer_size || !max_uploads_in_flight) { return ret; }

	if (!gpu || !gpu->is_init) { return ret; }

	gpu_buffer_ci ci = {0};
	{
		ci.memory_preference = GPU_BUFFER_MEMORY_MAPPED;
		ci.size = buffer_size;
		ci.usage = GPU_BUFFER_USAGE_STAGING_UP;
	}

	gpu_buffer staging_buf = gpu->create_buffer(gpu, &ci);
	if (!check_handle(staging_buf.handle)) {
		LOG_ERR("couldn't create staging buffer");
		return ret;
	}

	ret = arena_push_z(arena, transfer_queue);
	if (!ret) {
		LOG_ERR("couldn't allocate transfer queue");
		return ret;
	}

	transfer_task_array transfer_tasks =
		transfer_task_array_create(arena, max_uploads_in_flight);
	if (!transfer_tasks.raw) {
		LOG_ERR("couldn't allocate transfer_tasks queue");
		return ret;
	}
	transfer_task* first = transfer_task_array_begin(transfer_tasks);
	if (!first) {
		LOG_ERR("couldn't get first task");
		return ret;
	}

	uint64_t max_single_task_size = buffer_size / max_uploads_in_flight;
	{
		ret->gpu = gpu;

		ret->transfer_tasks = transfer_tasks;
		ret->current_task = 0;
		ret->tasks_in_flight = 0;

		ret->staging_buf = staging_buf;
		ret->total_usage_bytes = 0;
		ret->max_single_task_size = max_single_task_size;
	}
	_log_queue(ret);

	// Init 1st task:
	if (!_task_init(ret, first)) { LOG_ERR("couldn't init 1st task"); }

	return ret;
}

static bool _is_task_live(transfer_task* task) {
	return task && check_handle(task->cmd.handle) &&
		   check_handle(task->fence.handle);
}

void transfer_queue_destroy(transfer_queue* q) {
	// Make sure we haven't got anyting pending
	gpu_api* gpu = q->gpu;
	transfer_queue_batch_process_wait(q);
	transfer_queue_tick(q);

	transfer_task* task;
	if (!transfer_task_array_at(q->transfer_tasks, &task, q->current_task)) {
		LOG_ERR("couldn't get current task");
	}
	ASSERT(!_is_task_live(task));
	gpu->destroy_buffer(gpu, q->staging_buf);
}

static void _poll_task(transfer_queue* queue, transfer_task* task, bool wait) {
	gpu_api* gpu = queue->gpu;
	if (wait) {
		uint64_t const timeout = sec_to_ns(10);
		gpu->wait_fence(gpu, task->fence, timeout);
	} else if (!gpu->is_fence_signalled(gpu, task->fence)) {
		return;
	}
	// task finished: reclaim things
	queue->total_usage_bytes -= task->data_size;
	queue->tasks_in_flight--;
	gpu->destroy_command_buffer(gpu, task->cmd);
	gpu->destroy_fence(gpu, task->fence);
	task->data_size = 0;
	task->cmd = (gpu_command_buffer){POOL_INVALID_HANDLE};
	task->fence = (gpu_fence){POOL_INVALID_HANDLE};
}

bool transfer_queue_tick(transfer_queue* queue) {
	if (!queue) { return false; }

	if (queue->tasks_in_flight == 0) { return true; }

	transfer_task* task = transfer_task_array_begin(queue->transfer_tasks);
	uint64_t count = transfer_task_array_size(queue->transfer_tasks);

	uint32_t seen = 0;
	uint32_t should_see = queue->tasks_in_flight;
	for (uint32_t i = 0; i < count; i++) {
		if (seen >= should_see) { break; }

		if (_is_task_live(task)) {
			_poll_task(queue, task, false); // tick doesn't wait
			seen++;
		}

		task++;
	}

	LOG_DEBUG("%u tasks polled, %u completed, %u still pending", should_see,
			  should_see - queue->tasks_in_flight, queue->tasks_in_flight);

	return true;
}

static uint64_t _find_next_free_task(transfer_queue* q) {
	size_t arr_len = transfer_task_array_size(q->transfer_tasks);

	uint64_t idx = (q->current_task + 1) % arr_len;
	transfer_task* task;

	for (uint64_t i = 0; i < arr_len; i++) {
		if (!transfer_task_array_at(q->transfer_tasks, &task, idx)) {
			LOG_ERR("couldn't get next task");
			return UINT64_MAX;
		}
		if (!_is_task_live(task)) { return idx; }
		idx = (idx + 1) % arr_len;
	}

	return UINT64_MAX;
}

static bool _submit_task(transfer_queue* q, transfer_task* task) {
	ASSERT(_is_task_live(task));
	gpu_api* gpu = q->gpu;
	gpu->submit_command_buffer_fence(gpu, task->cmd, task->fence);

	q->tasks_in_flight++;
	LOG_DEBUG("task submitted (%u bytes). %u tasks in flight", task->data_size,
			  q->tasks_in_flight);

	uint64_t next_idx = _find_next_free_task(q);
	if (next_idx == UINT64_MAX) {
		LOG_ERR("couldn't get next task slot, queue is full");
		return false;
	}

	q->current_task = next_idx;
	transfer_task* next = NULL;
	if (!transfer_task_array_at(q->transfer_tasks, &next, q->current_task)) {
		LOG_ERR("couldn't get next task");
		return false;
	}

	return true;
}

bool transfer_queue_push_upload(transfer_queue* q,
								transfer_queue_upload upload) {
	if (!q) { return false; }
	if (q->tasks_in_flight == transfer_task_array_size(q->transfer_tasks)) {
		LOG_ERR("queue full, must stall");
		return false;
	}

	gpu_data_payload payload = upload.payload;
	size_t payload_size;
	uint8_t* payload_bytes = NULL;

	if (payload.data_format == GPU_DATA_SPAN) {
		payload_size = payload.data.span.size;
		payload_bytes = payload.data.span.data;
	} else {
		payload_size = u8_array_size(payload.data.array);
		payload_bytes = u8_array_begin(payload.data.array);
	}

	if (!payload_size || !payload_bytes) {
		LOG_ERR("invalid payload");
		return false;
	}

	if (payload_size > q->max_single_task_size) {
		LOG_ERR("TODO: implement chunking");
		return false;
	}

	transfer_task* task;
	transfer_task_array_at(q->transfer_tasks, &task, q->current_task);
	if (!task) {
		LOG_ERR("couldn't get current task");
		return false;
	}

	// 1st upload in this task, init command buffer & fence 1st
	if (!_is_task_live(task)) {
		if (!_task_init(q, task)) {
			LOG_ERR("couldn't initialize new task");
			return false;
		}
	}

	if (task->data_size + payload_size >= q->max_single_task_size) {
		// Submit current and begin new task
		if (!_submit_task(q, task)) { return false; }
		// current_task was updated after submission, get it again
		transfer_task_array_at(q->transfer_tasks, &task, q->current_task);
		if (!task) {
			LOG_ERR("couldn't get current task after submission");
			return false;
		}
		if (!_task_init(q, task)) {
			LOG_ERR("couldn't initialize new task");
			return false;
		}
	}

	// Push to current task
	gpu_api* gpu = q->gpu;
	size_t offset_in_staging = task->data_size;
	size_t offset_in_dest = payload.dst_offset;

	payload.dst_offset = offset_in_staging;
	gpu->copy_to_staging_buffer(gpu, q->staging_buf, payload);

	if (upload.type == TRQ_UPLOAD_BUFFER) {
		struct buf_upload* up = &upload.upload.buffer;
		gpu_buffer_copy_info copy_ci = {0};
		{
			copy_ci.size = payload_size;
			copy_ci.src = q->staging_buf;
			copy_ci.dst = up->dest;
			copy_ci.src_offset = task->data_size;
			copy_ci.dst_offset = offset_in_dest;
		}
		ASSERT(gpu->cmd_buffer_barrier(gpu, task->cmd, up->dest, up->acess, GPU_BUFFER_ACCESS_TRANSFER_WRITE)); // NOLINT
		ASSERT(gpu->cmd_copy_to_buffer(gpu, task->cmd, &copy_ci));
		ASSERT(gpu->cmd_buffer_barrier(gpu, task->cmd, up->dest, GPU_BUFFER_ACCESS_TRANSFER_WRITE, up->acess)); // NOLINT
	} else {
		struct img_upload* up = &upload.upload.image;
		gpu_image_copy_info copy_ci = {0};
		{
			copy_ci.src = q->staging_buf;
			copy_ci.src_offset = task->data_size;
			copy_ci.dst = up->dest;
			copy_ci.regions = up->regions;
		}

		ASSERT(gpu->cmd_transition_image(gpu, task->cmd, up->dest,
										 GPU_IMAGE_STATE_TRANSFER_DST));
		ASSERT(gpu->cmd_copy_to_image(gpu, task->cmd, &copy_ci));
		ASSERT(gpu->cmd_transition_image(gpu, task->cmd, up->dest, up->usage));
	}

	task->data_size += payload_size;
	q->total_usage_bytes += payload_size;

	return true;
}

bool transfer_queue_batch_process(transfer_queue* q) {
	if (!q) { return false; }

	transfer_task* task;
	if (!transfer_task_array_at(q->transfer_tasks, &task, q->current_task)) {
		LOG_ERR("couldn't get current task");
		return false;
	}

	if (!_is_task_live(task)) {
		// no-op, no uploads yet
		return true;
	}

	LOG_DEBUG("processing task slot %u", q->current_task);
	if (!_submit_task(q, task)) {
		LOG_ERR("couldn't submit current task");
		return false;
	}

	return true;
}

bool transfer_queue_batch_process_wait(transfer_queue* q) {
	if (!q) { return false; }

	// First, end current task
	transfer_task* task;
	if (!transfer_task_array_at(q->transfer_tasks, &task, q->current_task)) {
		LOG_ERR("couldn't get current task");
		return false;
	}

	if (_is_task_live(task)) {
		if (!_submit_task(q, task)) {
			LOG_ERR("couldn't submit current task");
			return false;
		}
	}

	// Iterate all tasks, and wait each fence
	task = transfer_task_array_begin(q->transfer_tasks);
	uint64_t count = transfer_task_array_size(q->transfer_tasks);

	uint32_t seen = 0;
	uint32_t should_see = q->tasks_in_flight;
	for (uint32_t i = 0; i < count; i++) {
		if (seen >= should_see) { break; }

		if (_is_task_live(task)) {
			_poll_task(q, task, true);
			seen++;
		}

		task++;
	}

	LOG_DEBUG("waited on %u tasks to complete. transfer queue empty", seen);
	ASSERT(q->tasks_in_flight == 0);
	ASSERT(q->total_usage_bytes == 0);

	return true;
}
