#ifndef QUEUE_H
#define QUEUE_H

#include <visibility.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct arena_t arena_t;

typedef struct queue_t {
	void* impl;
} queue_t;

LIB_EXPORT queue_t* queue_create(arena_t* arena, size_t max_size,
								 size_t elem_size);

LIB_EXPORT bool queue_enqueue(queue_t* queue, void* data);

LIB_EXPORT bool queue_dequeue(queue_t* queue, void* ret);

LIB_EXPORT bool queue_full(queue_t* queue);

LIB_EXPORT bool queue_empty(queue_t* queue);

LIB_EXPORT size_t queue_get_usage(queue_t* queue);

LIB_EXPORT size_t queue_get_remaining(queue_t* queue);

LIB_EXPORT void queue_clear(queue_t* queue);

#endif // !QUEUE_H
