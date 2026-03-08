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

/**
 * @brief Create a new queue.
 *
 * @param arena The arena to allocate memory.
 * @param max_size Maximum size of the queue.
 * @param elem_size Size of a single element.
 * @return The new queue, or NULL on failure.
 */
LIB_EXPORT queue_t* queue_create(arena_t* arena, size_t max_size,
								 size_t elem_size);

/**
 * @brief Push an element to the queue. The element will be copied.
 *
 * @return False if invalid queue or data, true otherwise.
 */
LIB_EXPORT bool queue_enqueue(queue_t* queue, void* data);

/**
 * @brief Pop an element from the queue. The element will be copied.
 *
 * @param ret Pointer where result will be copied.
 * @return False if invalid queue or out pointer, true otherwise.
 */
LIB_EXPORT bool queue_dequeue(queue_t* queue, void* ret);

/**
 * @brief Checks if the queue is full.
 */
LIB_EXPORT bool queue_full(queue_t* queue);

/**
 * @brief Checks if the queue is empty.
 */
LIB_EXPORT bool queue_empty(queue_t* queue);

/**
 * @brief Gets the number of items in the queue.
 */
LIB_EXPORT size_t queue_get_usage(queue_t* queue);

/**
 * @brief Gets the number of items that can be pushed before the queue is full.
 */
LIB_EXPORT size_t queue_get_remaining(queue_t* queue);

/**
 * @brief Clears the queue. Memory is zeroed-out and size is reset.
 */
LIB_EXPORT void queue_clear(queue_t* queue);

// Type safe wrapper. Use for copied types:
#define QUEUE_DEFINE_TYPE(T, Name)                                             \
                                                                               \
	typedef struct Name {                                                      \
		queue_t* raw;                                                          \
	} Name;                                                                    \
                                                                               \
	__attribute__((unused)) static inline Name Name##_create(                  \
		arena_t* arena, size_t max_size) {                                     \
		Name q = {queue_create(arena, max_size, sizeof(T))};                   \
		return q;                                                              \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_enqueue(Name q,          \
															  T* val) {        \
		return queue_enqueue(q.raw, val);                                      \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_dequeue(Name q,          \
															  T* out) {        \
		return queue_dequeue(q.raw, out);                                      \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_full(Name q) {           \
		return queue_full(q.raw);                                              \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_empty(Name q) {          \
		return queue_empty(q.raw);                                             \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline size_t Name##_size(Name q) {         \
		return queue_get_usage(q.raw);                                         \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline size_t Name##_remaining(Name q) {    \
		return queue_get_remaining(q.raw);                                     \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline void Name##_clear(Name q) {          \
		queue_clear(q.raw);                                                    \
	}

// Pointer version, use for storing pointers to objects:
#define QUEUE_DEFINE_PTR_TYPE(T, Name)                                         \
                                                                               \
	typedef struct Name {                                                      \
		queue_t* raw;                                                          \
	} Name;                                                                    \
                                                                               \
	__attribute__((unused)) static inline Name Name##_create(                  \
		arena_t* arena, size_t max_size) {                                     \
		Name q = {queue_create(arena, max_size, sizeof(void*))};               \
		return q;                                                              \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_enqueue(Name q,          \
															  T* val) {        \
		void* ptr = (void*)val;                                                \
		return queue_enqueue(q.raw, &ptr);                                     \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline T* Name##_dequeue(Name q) {          \
		void* ptr = NULL;                                                      \
		if (!queue_dequeue(q.raw, &ptr)) return NULL;                          \
		return (T*)ptr;                                                        \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_full(Name q) {           \
		return queue_full(q.raw);                                              \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_empty(Name q) {          \
		return queue_empty(q.raw);                                             \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline size_t Name##_size(Name q) {         \
		return queue_get_usage(q.raw);                                         \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline size_t Name##_remaining(Name q) {    \
		return queue_get_remaining(q.raw);                                     \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline void Name##_clear(Name q) {          \
		queue_clear(q.raw);                                                    \
	}
#endif // !QUEUE_H
