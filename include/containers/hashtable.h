#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdint.h>
#include <visibility.h>

#include <stddef.h>
#include <stdbool.h>

typedef struct hashtable_t hashtable_t;
typedef struct arena_t arena_t;
typedef struct string string;

typedef size_t (*hash_fn)(string* key, uint32_t elem_count);
LIB_EXPORT size_t hash_default(string* key, uint32_t elem_count);

#define HT_RATIO .8f


//note: bigger than count*elem_size !!
LIB_EXPORT hashtable_t* hashtable_create(arena_t* arena, uint32_t count,
										 uint32_t elem_size, hash_fn func);

// insert a copy of what's stored at elem
LIB_EXPORT bool hashtable_insert(hashtable_t* ht, string* key, void* elem);

// get a copy of what's stored at elem
LIB_EXPORT bool hashtable_get(hashtable_t* ht, void* out, string* key);

LIB_EXPORT uint32_t hashtable_usage(hashtable_t* ht);

LIB_EXPORT void hashtable_reset(hashtable_t* ht);

#endif // !HASHTABLE_H
