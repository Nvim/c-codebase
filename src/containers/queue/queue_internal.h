#ifndef QUEUE_INTERNAL_H
#define QUEUE_INTERNAL_H

#include <allocators/arena.h>
#include <containers/queue.h>
#include <core/log.h>
#include <platform/platform.h>
#include <core/core.h>

typedef struct queue_impl {
	void* data;
	size_t buffer_size;
	size_t start;
	size_t current; // 1st FREE slot
	size_t alloc_count;
	size_t elem_size;
	size_t _pad[2];
} queue_impl;

STATIC_ASSERT(sizeof(queue_impl) == 64, "Bad queue size");

#endif // !QUEUE_INTERNAL_H
