#ifndef STRING_H
#define STRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <visibility.h>

typedef struct string_t {
	char* str;
	size_t len;
} string;

typedef struct arena_t arena_t;

LIB_EXPORT bool string_create(arena_t* arena, string* out, char const* cstr);

LIB_EXPORT bool string_copy(arena_t* arena, string* out, string* s);

// new string
LIB_EXPORT bool string_append(arena_t* arena, string* out, string* s, char c);

// new string
LIB_EXPORT bool string_concat(arena_t* arena, string* out, string* s1,
							  string* s2);

// pos or len
LIB_EXPORT size_t string_contains(string* s, string* pattern);

// compare content
LIB_EXPORT bool string_compare(string* s1, string* s2);

// compare addr only, not len nor contents
LIB_EXPORT bool string_compare_addr(string* s1, string* s2);

// replacement optional
LIB_EXPORT bool string_replace(arena_t* arena, string* out, string* s,
							   string* pattern, string* replacement);

#endif // !STRING_H
