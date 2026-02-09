#ifndef STRING_H
#define STRING_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <visibility.h>

typedef struct string_t {
	char* str;
	size_t len;
} string;

typedef struct arena_t arena_t;

static size_t const STRING_NPOS = UINT64_MAX;

LIB_EXPORT bool string_create(arena_t* arena, string* out, char const* cstr);

LIB_EXPORT bool string_copy(arena_t* arena, string* out, string* s);

// new string
LIB_EXPORT bool string_append(arena_t* arena, string* out, string* s, char c);

// new string
LIB_EXPORT bool string_concat(arena_t* arena, string* out, string* s1,
							  string* s2);
LIB_EXPORT bool string_concat_cstr(arena_t* arena, string* out, string* s1,
								   char const* cstr);

// pos or npos
LIB_EXPORT size_t string_contains(string* s, string* pattern);
LIB_EXPORT size_t string_contains_cstr(string* s, char const* pattern);

// compare content
LIB_EXPORT bool string_compare(string* s1, string* s2);

// compare addr only, not len nor contents
LIB_EXPORT bool string_compare_addr(string* s1, string* s2);

// replacement optional
LIB_EXPORT bool string_replace(arena_t* arena, string* out, string* s,
							   string* pattern, string* replacement);

#endif // !STRING_H
