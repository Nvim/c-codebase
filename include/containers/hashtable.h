#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <strings/string.h>
#include <visibility.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hashtable_t hashtable_t;
typedef struct arena_t arena_t;

typedef size_t (*hash_fn)(string* key, uint32_t elem_count);
LIB_EXPORT size_t hash_default(string* key, uint32_t elem_count);

#define HT_RATIO .8f

/**
 * @brief Create a new hashtable.
 * @note The table will be larger than count*elem_size in order to have a max
 * fill ratio of HT_RATIO.
 *
 * @param arena Arena to allocate the table from.
 * @param count Max number of elements expected to hold.
 * @param elem_size Size of each element.
 * @param func Hashing function.
 * @return The hashtable, or NULL on failure.
 */
LIB_EXPORT hashtable_t* hashtable_create(arena_t* arena, uint32_t count,
										 uint32_t elem_size, hash_fn func);

/**
 * @brief Insert an element into the hashtable by copying it. Existing keys will
 * trigger an error and data won't be squashed.
 *
 * @return False on invalid parameters or collision, true otherwise.
 */
LIB_EXPORT bool hashtable_insert(hashtable_t* ht, string key, void* elem);

/**
 * @brief Retrieve an element by copying it.
 *
 * @return False on invalid parameters or missing element, true otherwise.
 */
LIB_EXPORT bool hashtable_get(hashtable_t* ht, void* out, string key);

/**
 * @brief Retrieve the amount of entries in the hashtable.
 *
 */
LIB_EXPORT uint32_t hashtable_usage(hashtable_t* ht);

/**
 * @brief Clears the hashtable.
 *
 */
LIB_EXPORT void hashtable_reset(hashtable_t* ht);

// Type safe wrapper. Use with copied types.
#define HASHTABLE_DEFINE_TYPE(T, Name)                                         \
                                                                               \
	typedef struct Name {                                                      \
		hashtable_t* raw;                                                      \
	} Name;                                                                    \
                                                                               \
	__attribute__((unused)) static inline Name Name##_create(                  \
		arena_t* arena, uint32_t count, hash_fn func) {                        \
		Name ht = {hashtable_create(arena, count, sizeof(T), func)};           \
		return ht;                                                             \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_insert(                  \
		Name ht, string key, T elem) {                                         \
		return hashtable_insert(ht.raw, key, &elem);                           \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_get(Name ht, T* out,     \
														  string key) {        \
		return hashtable_get(ht.raw, out, key);                                \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline uint32_t Name##_usage(Name ht) {     \
		return hashtable_usage(ht.raw);                                        \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline void Name##_reset(Name ht) {         \
		hashtable_reset(ht.raw);                                               \
	}

// Pointer version, use for storing pointers to objects:
#define HASHTABLE_DEFINE_PTR_TYPE(T, Name)                                     \
                                                                               \
	typedef struct Name {                                                      \
		hashtable_t* raw;                                                      \
	} Name;                                                                    \
                                                                               \
	__attribute__((unused)) static inline Name Name##_create(                  \
		arena_t* arena, uint32_t count, hash_fn func) {                        \
		Name ht = {hashtable_create(arena, count, sizeof(void*), func)};       \
		return ht;                                                             \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline bool Name##_insert(                  \
		Name ht, string key, T* elem) {                                        \
		void* ptr = (void*)elem;                                               \
		return hashtable_insert(ht.raw, key, &ptr);                            \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline T* Name##_get(Name ht, string key) { \
		void* ptr = NULL;                                                      \
		if (!hashtable_get(ht.raw, &ptr, key)) return NULL;                    \
		return (T*)ptr;                                                        \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline uint32_t Name##_usage(Name ht) {     \
		return hashtable_usage(ht.raw);                                        \
	}                                                                          \
                                                                               \
	__attribute__((unused)) static inline void Name##_reset(Name ht) {         \
		hashtable_reset(ht.raw);                                               \
	}

#endif // !HASHTABLE_H
