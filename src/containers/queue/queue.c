#include "queue_internal.h"

#include <allocators/arena.h>
#include <containers/queue.h>
#include <core/log.h>
#include <core/platform.h>

#include <string.h>

static bool _queue_full(queue_impl* q) {
	return q->alloc_count >= q->buffer_size;
}

static bool _queue_empty(queue_impl* q) { return q->alloc_count == 0; }

static void* _queue_get_at(queue_impl* q, size_t idx) {
	return (char*)q->data + (idx * q->elem_size);
}

queue_t* queue_create(arena_t* arena, size_t max_size, size_t elem_size) {
	if (!arena || max_size == 0 || elem_size == 0) {
		LOG_ERR("queue_create: invalid parameters");
		return NULL;
	}

	size_t buffer_size = elem_size * max_size;
	if (arena_get_remaining(arena) < sizeof(queue_impl) + buffer_size) {
		LOG_ERR("queue_create: not enough space left in arena");
		return NULL;
	}

	arena_checkpoint* cp = arena_get_checkpoint(arena);
	queue_impl* impl = arena_alloc(arena, sizeof(queue_impl));
	if (!impl) {
		LOG_ERR("queue_create: couldn't allocate queue header");
		return NULL;
	}

	impl->data = arena_alloc(arena, buffer_size);
	if (!impl) {
		LOG_ERR("queue_create: couldn't allocate queue buffer");
		arena_restore_checkpoint(arena, cp, true);
		return NULL;
	}

	impl->buffer_size = max_size;
	impl->elem_size = elem_size;
	impl->current = 0;
	impl->start = 0;
	impl->alloc_count = 0;

	return (queue_t*)impl;
}

bool queue_enqueue(queue_t* queue, void* data) {
	if (!queue || !data) {
		LOG_ERR("queue_push: invalid parameters");
		return false;
	}

	queue_impl* impl = (queue_impl*)queue;
	if (_queue_full(impl)) {
		LOG_ERR("queue_push: queue is full");
		return false;
	}

	size_t idx = impl->current++ % impl->buffer_size;
	void* addr = _queue_get_at(impl, idx);
	memcpy(addr, data, impl->elem_size);

	impl->alloc_count++;

	return true;
}

bool queue_dequeue(queue_t* queue, void* ret) {
	if (!queue || !ret) {
		LOG_ERR("queue_pop: invalid parameters");
		return false;
	}

	queue_impl* impl = (queue_impl*)queue;
	if (_queue_empty(impl)) {
		LOG_ERR("queue_push: queue is empty");
		return false;
	}

	size_t idx = impl->start++ % impl->buffer_size;
	void* addr = _queue_get_at(impl, idx);
	memcpy(ret, addr, impl->elem_size);

	impl->alloc_count--;

	return true;
}

LIB_EXPORT bool queue_full(queue_t* queue) {
	if (!queue) {
		LOG_ERR("queue_get_usage: invalid parameter");
		return false;
	}
	return _queue_full((queue_impl*)queue);
}

LIB_EXPORT bool queue_empty(queue_t* queue) {
	if (!queue) {
		LOG_ERR("queue_get_usage: invalid parameter");
		return false;
	}
	return _queue_empty((queue_impl*)queue);
}

size_t queue_get_usage(queue_t* queue) {
	if (!queue) {
		LOG_ERR("queue_get_usage: invalid parameter");
		return 0;
	}

	queue_impl* impl = (queue_impl*)queue;
	return impl->alloc_count;
}

size_t queue_get_remaining(queue_t* queue) {
	if (!queue) {
		LOG_ERR("queue_get_usage: invalid parameter");
		return 0;
	}

	queue_impl* impl = (queue_impl*)queue;
	return impl->buffer_size - impl->alloc_count;
}

void queue_clear(queue_t* queue) {
	if (!queue) {
		LOG_ERR("queue_clear: invalid parameter");
		return;
	}
	queue_impl* impl = (queue_impl*)queue;
	memset(impl->data, 0, impl->elem_size * impl->buffer_size);

	impl->alloc_count = 0;
	impl->start = 0;
	impl->current = 0;
}
