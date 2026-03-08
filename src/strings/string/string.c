#include <strings/string.h>

#include "array_internal.h"
#include <allocators/arena.h>
#include <containers/array_types.h>
#include <containers/queue.h>
#include <core/log.h>
#include <core/platform.h>
#include <ctype.h>
#include <string.h>

STATIC_ASSERT(sizeof(string) == 16, "unexpected string size");
#define STR_HEADER_SIZE sizeof(string)

bool string_create(arena_t* arena, string* out, const char* cstr) {
	if (!arena || !out || !cstr) {
		LOG_ERR("string_create: invalid parameters");
		return false;
	}

	size_t len = strlen(cstr);
	if (!len) {
		LOG_ERR("string_create: can't create empty string");
		return false;
	}

	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_create: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, cstr, len);
	out->len = len;
	out->str = buf;

	return true;
}

bool string_create_fmt(arena_t* arena, string* out, char const* cstr, ...) {
	if (!arena || !out || !cstr) {
		LOG_ERR("string_create: invalid parameters");
		return false;
	}

	/* Determine required size.  */
	int n = 0;
	va_list ap;
	va_start(ap, cstr);
	n = vsnprintf(NULL, 0, cstr, ap);
	va_end(ap);

	if (n < 0) {
		LOG_ERR("string_create_fmt: couldn't compute string length");
		return false;
	}

	size_t len = n;
	if (!len) {
		LOG_ERR("string_create_fmt: can't create empty string");
		return false;
	}

	char* buf = arena_alloc(arena, len + 1);
	if (!buf) {
		LOG_ERR("string_create_fmt: couldn't allocate new string buffer");
		return false;
	}

	va_start(ap, cstr);
	n = vsnprintf(buf, len + 1, cstr, ap);
	va_end(ap);

	if (n < 0) {
		LOG_ERR("string_create_fmt: couldn't format string to buffer");
		return false;
	}

	ASSERT((size_t)n == len);

	out->len = len;
	out->str = buf;

	return true;
}

bool string_copy(arena_t* arena, string* out, string s) {
	if (!arena || !out || !s.len) {
		LOG_ERR("string_copy: invalid parameters");
		return false;
	}

	size_t len = s.len;
	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_copy: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, s.str, s.len);
	out->len = len;
	out->str = buf;

	return out;
}

bool string_append(arena_t* arena, string* out, string s, char c) {
	if (!arena || !out || !s.len) {
		LOG_ERR("string_append: invalid parameters");
		return false;
	}

	size_t len = s.len + 1;
	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_append: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, s.str, s.len);
	buf[s.len] = c;
	out->len = len;
	out->str = buf;

	return out;
}

bool string_concat(arena_t* arena, string* out, string s1, string s2) {
	if (!arena || !out || !s1.len || !s2.len) {
		LOG_ERR("string_concat: invalid parameters");
		return false;
	}
	size_t len = s1.len + s2.len;
	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_concat: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, s1.str, s1.len);
	memcpy(buf + s1.len, s2.str, s2.len);
	out->len = len;
	out->str = buf;

	return true;
}

bool string_concat_cstr(arena_t* arena, string* out, string s1,
						char const* cstr) {
	if (!arena || !out || !s1.len || !cstr) {
		LOG_ERR("string_concat_cstr: invalid parameters");
		return false;
	}

	size_t cstr_len = strlen(cstr);
	size_t len = s1.len + cstr_len;
	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_concat_cstr: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, s1.str, s1.len);
	memcpy(buf + s1.len, cstr, cstr_len);
	out->len = len;
	out->str = buf;

	return out;
}

size_t string_contains(string s, string pattern) {
	size_t s_len = s.len;
	size_t p_len = pattern.len;
	if (!s_len || !p_len) { return STRING_NPOS; }
	if (s_len < p_len) { return STRING_NPOS; }
	if (s_len == p_len) { return string_compare(s, pattern) ? 0 : STRING_NPOS; }

	for (size_t i = 0; i <= s_len - p_len; i++) {
		bool good = true;
		for (size_t j = 0; j < p_len; j++) {
			if ((i + j >= s_len) || s.str[i + j] != pattern.str[j]) {
				good = false;
				break;
			}
		}
		if (good) { return i; }
	}
	return STRING_NPOS;
}

size_t string_contains_cstr(string s, char const* pattern) {
	size_t s_len = s.len;
	size_t p_len = strlen(pattern);
	if (!s_len || !p_len) { return STRING_NPOS; }
	if (s_len < p_len) { return STRING_NPOS; }
	if (s_len == p_len) {
		return (strncmp(s.str, pattern, p_len) == 0) ? 0 : STRING_NPOS;
	}

	for (size_t i = 0; i <= s_len - p_len; i++) {
		bool good = true;
		for (size_t j = 0; j < p_len; j++) {
			if ((i + j >= s_len) || s.str[i + j] != pattern[j]) {
				good = false;
				break;
			}
		}
		if (good) { return i; }
	}
	return STRING_NPOS;
}

size_t string_contains_last(string s, string pattern) {
	size_t s_len = s.len;
	size_t p_len = pattern.len;
	if (!s_len || !p_len) { return STRING_NPOS; }
	if (s_len < p_len) { return STRING_NPOS; }
	if (s_len == p_len) { return string_compare(s, pattern) ? 0 : STRING_NPOS; }

	for (size_t i = s_len - p_len + 1; i-- > 0;) {
		bool good = true;
		for (size_t j = 0; j < p_len; j++) {
			if ((i + j >= s_len) || s.str[i + j] != pattern.str[j]) {
				good = false;
				break;
			}
		}
		if (good) { return i; }
	}
	return STRING_NPOS;
}

size_t string_contains_last_cstr(string s, char const* pattern) {
	size_t s_len = s.len;
	size_t p_len = strlen(pattern);
	if (!s_len || !p_len) { return STRING_NPOS; }
	if (s_len < p_len) { return STRING_NPOS; }
	if (s_len == p_len) {
		return (strncmp(s.str, pattern, p_len) == 0) ? 0 : STRING_NPOS;
	}

	for (size_t i = s_len - p_len + 1; i-- > 0;) {
		bool good = true;
		for (size_t j = 0; j < p_len; j++) {
			if ((i + j >= s_len) || s.str[i + j] != pattern[j]) {
				good = false;
				break;
			}
		}
		if (good) { return i; }
	}
	return STRING_NPOS;
}

bool string_compare(string s1, string s2) {
	if (s1.len != s2.len) { return false; }

	if (s1.str == s2.str) { return true; }

	for (size_t i = 0; i < s1.len; i++) {
		if (s1.str[i] != s2.str[i]) { return false; }
	}
	return true;
}

bool string_compare_addr(string s1, string s2) { return (s1.str == s2.str); }

bool string_occurences(arena_t* arena, arena_t* scratch, u64_array* out, string s,
					   string pattern) {
	if (!arena || !out || !s.len || !pattern.len) {
		LOG_ERR("string_occurences: invalid parameters");
		return false;
	}

	if (pattern.len >= s.len) { return false; }

	// Create scratch arena for queue:
	size_t p_len = pattern.len;
	size_t max_occurences = s.len / p_len;
	ASSERT(max_occurences != 0);

	queue_t* queue = queue_create(scratch, max_occurences, sizeof(size_t));
	if (!queue) {
		LOG_ERR("string_occurences: couldn't create temp queue");
		return false;
	}

	size_t i = 0;
	while (i < s.len && s.len - i >= p_len) {
		string tmp = {
			.str = &s.str[i],
			.len = s.len - i,
		};

		size_t match_idx = string_contains(tmp, pattern);
		if (match_idx != STRING_NPOS) {
			size_t m = match_idx + i;
			queue_enqueue(queue, &m);
			i = match_idx + i + p_len;
			// tmp.len = match_idx;
			// if (tmp.len) { }
		} else {
			break;
		}
	}

	size_t count = queue_get_usage(queue);
	if (!count) {
		// Nothing to return
		return false;
	}

	// Create result array with the right size:
	{
		*out = u64_array_create(arena, count);
		if (!out) {
			LOG_ERR("string_occurences: couldn't allocate result array");
			return false;
		}

		// Fill result:
		for (size_t idx = 0; idx < count; ++idx) {
			size_t elem;
			ASSERT(queue_dequeue(queue, &elem));
			// LOG_INFO("queue: %zu", elem);
			if (!u64_array_set(*out, idx, &elem)) {
				LOG_ERR("string_occurences: error filling result array");
				return false;
			}
			// size_t idk;
			// LOG_INFO("arr: %zu", u64_array_get(*out, &idk, i));
		}
		ASSERT(queue_empty(queue));
	}

	return true;
}

bool string_tokenize(arena_t* arena, string_array* out, string s,
					 string delim) {
	if (!arena || !out || !s.len || !delim.len) {
		LOG_ERR("string_tokenize: invalid parameters");
		return false;
	}

	if (delim.len >= s.len) { return false; }

	size_t first_occ = string_contains(s, delim);
	if (first_occ == STRING_NPOS) { return false; }

	size_t pos = 0;

	if (first_occ == 0) {
		// string starts with pattern
		pos = delim.len;
	}

	size_t max_count = s.len / delim.len + 1;
	string_array arr = string_array_create(arena, max_count);
	if (string_array_size(arr) == 0) { return false; }

	size_t elem_count = 0;
	while (pos < s.len) {
		string ss = {
			.len = s.len - pos,
			.str = &s.str[pos],
		};
		size_t next_occ = string_contains(ss, delim);
		if (next_occ == STRING_NPOS) {
			// no more occurences, append last chars:
			string ret = {
				.len = s.len - pos,
				.str = &s.str[pos],
			};
			if (ret.len == 0) { ret.str = NULL; }
			string_array_set(arr, elem_count++, &ret);
			break;
		}

		string ret = {
			.len = next_occ,
			.str = &s.str[pos],
		};
		if (ret.len == 0) { ret.str = NULL; }
		string_array_set(arr, elem_count++, &ret);

		if (next_occ + delim.len + pos >= s.len) {
			// occurence was last & trailing
			break;
		}

		pos += (next_occ + delim.len);
	}

	array_impl* arr_head = (array_impl*)arr.raw;
	arr_head->size = elem_count;

	*out = arr;

	return true;
}

bool string_replace(arena_t* arena, arena_t* scratch, string* out, string s, string pattern,
					string replacement) {
	if (!arena || !out || !s.len) {
		LOG_ERR("string_replace: invalid parameters");
		return false;
	}

	if (string_compare(s, pattern)) {
		return string_copy(arena, out, replacement);
	}

	// Bad pattern -> return s untouched
	if (!pattern.len || pattern.len > s.len || !pattern.len) {
		return string_copy(arena, out, s);
	}

	// Build array of occurences indices:
	size_t p_len = pattern.len;
	size_t max_occurences = s.len / p_len;
	ASSERT(max_occurences != 0);

	size_t* indices =
		arena_alloc_array(scratch, sizeof(size_t), max_occurences);
	if (!indices) {
		LOG_ERR("string_replace: couldn't allocate temp array for %zu "
				"max_occurences",
				max_occurences);
		return false;
	}

	size_t occurences = 0;
	size_t i = 0;
	while (i < s.len && s.len - i >= p_len) {
		string tmp = {
			.str = &s.str[i],
			.len = s.len - i,
		};

		size_t match_idx = string_contains(tmp, pattern);
		if (match_idx != STRING_NPOS) {
			indices[occurences++] = match_idx + i;
			i = match_idx + i + 1;
		} else {
			break;
		}
	}

	if (!occurences) {
		LOG_INFO("string_replace: no occurences of pattern");
		return string_copy(arena, out, s); // No match isn't a failure
	}

	// Now that we know occurences, we can build result:
	size_t r_len = replacement.len ? replacement.len : 0;
	size_t buf_len = s.len - (p_len * occurences) + (r_len * occurences);

	char* buf = arena_alloc_array(arena, 1, buf_len);
	if (!buf) {
		LOG_ERR("string_replace: couldn't allocate string buffer");
		return false;
	}

	size_t current_occ = 0;
	i = 0;
	size_t s_idx = 0;
	while (i < buf_len) {
		ASSERT(current_occ <= occurences);
		if (s_idx == indices[current_occ]) {
			memcpy(&buf[i], replacement.str, r_len);
			current_occ++;
			i += r_len;
			s_idx += p_len;
		} else {
			buf[i++] = s.str[s_idx++];
		}
	}

	out->len = buf_len;
	out->str = buf;
	return true;
}

bool string_ltrim(string* str, size_t n) {
	if (!str || n >= str->len) {
		LOG_ERR("string_ltrim: invalid parameters");
		return false;
	}

	str->str += n;
	str->len -= n;
	return true;
}

bool string_rtrim(string* str, size_t n) {
	if (!str || n >= str->len) {
		LOG_ERR("string_rtrim: invalid parameters");
		return false;
	}

	str->len -= n;
	return true;
}

bool string_trimspace(string* str) {
	if (!str) {
		LOG_ERR("string_trimspace: invalid parameter");
		return false;
	}
	size_t i = 0;
	size_t j = str->len - 1;

	while (isspace(str->str[i]) != 0) { i++; }
	while (isspace(str->str[j]) != 0) { j--; }

	if (i >= j) {
		LOG_ERR("string_trimspace: empty string");
		return false;
	}

	str->len -= (str->len - j - 1);
	str->len -= i;
	str->str += i;

	return true;
}
