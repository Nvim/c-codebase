#ifndef STRING_H
#define STRING_H

#include <containers/array_types.h>
#include <containers/queue_types.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <visibility.h>

typedef struct string {
	char* str;
	size_t len;
} string;

typedef struct arena_t arena_t;
typedef struct string_array string_array;
typedef struct u64_array u64_array;

/**
 * @brief Turns a temporary string literal into a length-based string
 */
#define str(literal) (string){(char*)literal, countof(literal) - 1}

/**
 * @brief Invalid position returned by search functions.
 */
static size_t const STRING_NPOS = UINT64_MAX;

/**
 * @note Printing length-based strings requires specific printf specifier to
 * only print up to a specific length, since string is no null terminated:
 * printf("string: %.*s", (int)s.len, s.str);
 * The following macros allow easier syntax:
 * printf("string is: " STR_FMT, STR_FMT_ARGS(s));
 */
#define STR_FMT "%.*s"
#define STR_FMT_ARGS(s) (int)s.len, s.str

ARRAY_DEFINE_TYPE(string, string_array)
QUEUE_DEFINE_TYPE(string, string_queue)

/**
 * @brief Creates a new string.
 *
 * @param arena The arena to allocate from.
 * @param out The resulting string.
 * @param cstr String data to create. Can't be empty.
 * @return false if empty parameters or allocation failure.
 */
LIB_EXPORT bool string_create(arena_t* arena, string* out, char const* cstr);

/**
 * @brief Creates a new string with formatting.
 *
 * @param arena The arena to allocate from.
 * @param out The resulting string.
 * @param cstr String data, followed by formatting arguments. Can't be empty.
 * @return false if empty parameters or allocation failure.
 */
LIB_EXPORT bool string_create_fmt(arena_t* arena, string* out, char const* cstr,
								  ...);

/**
 * @brief Copies an existing string
 *
 * @param arena The arena to allocate from.
 * @param out The resulting string.
 * @param s The string to copy.
 * @return false if empty parameters or allocation failure
 */
LIB_EXPORT bool string_copy(arena_t* arena, string* out, string s);

/**
 * @brief Append a character to a string. The original string is unchanged and a
 * copy is created.
 *
 * @param arena The arena to allocate from.
 * @param out The resulting string.
 * @param s The string to append to.
 * @param c The character to append.
 * @return false if empty parameters or allocation failure.
 */
LIB_EXPORT bool string_append(arena_t* arena, string* out, string s, char c);

/**
 * @brief Concatenate two strings. The original strings are unchanged and a new
 * one is created.
 *
 * @param arena The arena to allocate from.
 * @param out The resulting string.
 * @param s1 Prefix.
 * @param s2 Suffix.
 * @return false if empty parameters or allocation failure.
 */
LIB_EXPORT bool string_concat(arena_t* arena, string* out, string s1,
							  string s2);

/**
 * @brief Concatenate two strings. The original strings are unchanged and a new
 * one is created.
 *
 * @param arena The arena to allocate from.
 * @param out The resulting string.
 * @param s1 Prefix.
 * @param s2 Suffix.
 * @return false if empty parameters or allocation failure.
 */
LIB_EXPORT bool string_concat_cstr(arena_t* arena, string* out, string s1,
								   char const* cstr);

/**
 * @brief Find a substring.
 *
 * @param s The string to search into.
 * @param pattern The substring to look for.
 * @return Index of first occurence, or `STRING_NPOS` on failure.
 */
LIB_EXPORT size_t string_contains(string s, string pattern);

/**
 * @brief Find a substring.
 *
 * @param s The string to search into.
 * @param pattern The substring to look for.
 * @return Index of first occurence, or `STRING_NPOS` on failure.
 */
LIB_EXPORT size_t string_contains_cstr(string s, char const* pattern);

/**
 * @brief Find a substring from the end.
 *
 * @param s The string to search into.
 * @param pattern The substring to look for.
 * @return Index of last occurence, or `STRING_NPOS` on failure.
 */
LIB_EXPORT size_t string_contains_last(string s, string pattern);

/**
 * @brief Find a substring from the end.
 *
 * @param s The string to search into.
 * @param pattern The substring to look for.
 * @return Index of last occurence, or `STRING_NPOS` on failure.
 */
LIB_EXPORT size_t string_contains_last_cstr(string s, char const* pattern);

/**
 * @brief Compare the contents of two strings.
 */
LIB_EXPORT bool string_compare(string s1, string s2);

/**
 * @brief Compare the addresses of the underlying char buffers.
 * @note Length and contents aren't compared.
 */
LIB_EXPORT bool string_compare_addr(string s1, string s2);

LIB_EXPORT bool string_occurences(arena_t* arena, arena_t* scratch,
								  u64_array* out, string s, string pattern);

/**
 * @brief Tokenzie a string based on delimiter.
 *
 * @param arena Arena to allocate the resulting array from..
 * @param out Array that will contain all tokens. Invalid if false is returned
 * @param s String to tokenize.
 * @param delim Separator between tokens.
 * @return false on invalid parameters or allocation failure, true otherwise.
 */
LIB_EXPORT bool string_tokenize(arena_t* arena, string_array* out, string s,
								string delim);

/**
 * @brief Replace all occurences of a pattern in the string. The replacement can
 * be empty to delete occurences of pattern instead.
 *
 * @param arena The arena to allocate from.
 * @param out The resulting string.
 * @param s Original string to perform the replacement.
 * @param pattern Pattern to replace.
 * @param replacement Optional replacement string.
 * @return false on allocation failure or empty parameters. An empty replacement
 * is valid, and empty pattern will noop silently.
 */
LIB_EXPORT bool string_replace(arena_t* arena, arena_t* scratch, string* out,
							   string s, string pattern, string replacement);

/**
 * @brief Trims the first characters of a string.
 * @note No allocation is performed. Pointer is simply moved.
 *
 * @param str The string to trim.
 * @param n The number of characters to trim.
 * @return false if `str` is empty, or `n` is too large.
 */
LIB_EXPORT bool string_ltrim(string* str, size_t n);

/**
 * @brief Trims the last characters of a string.
 * @note No allocation is performed. Length is simply changed.
 *
 * @param str The string to trim.
 * @param n The number of characters to trim.
 * @return false if `str` is empty, or `n` is too large.
 */
LIB_EXPORT bool string_rtrim(string* str, size_t n);

/**
 * @brief Removes leading and trailing whitespace from a string. Whitespace is
 * checked with POSIX `isspace`.
 * @note No allocation is performed. Pointer and length are simply changed.
 *
 * @param str The string to trim.
 * @return false if `str` is empty.
 */
LIB_EXPORT bool string_trimspace(string* str);

#endif // !STRING_H
