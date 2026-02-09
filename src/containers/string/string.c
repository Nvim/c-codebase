#include <containers/arena.h>
#include <containers/string.h>
#include <core/log.h>
#include <core/platform.h>
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

bool string_copy(arena_t* arena, string* out, string* s) {
	if (!arena || !out || !s) {
		LOG_ERR("string_copy: invalid parameters");
		return false;
	}

	if (!s->len) {
		LOG_ERR("string_copy: can't copy empty string");
		return false;
	}

	size_t len = s->len;
	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_copy: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, s->str, s->len);
	out->len = len;
	out->str = buf;

	return out;
}

bool string_append(arena_t* arena, string* out, string* s, char c) {
	if (!arena || !out || !s) {
		LOG_ERR("string_append: invalid parameters");
		return false;
	}

	size_t len = s->len + 1;
	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_append: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, s->str, s->len);
	buf[s->len] = c;
	out->len = len;
	out->str = buf;

	return out;
}

bool string_concat(arena_t* arena, string* out, string* s1, string* s2) {
	if (!arena || !out || !s1 || !s2) {
		LOG_ERR("string_concat: invalid parameters");
		return false;
	}
	size_t len = s1->len + s2->len;
	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_concat: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, s1->str, s1->len);
	memcpy(buf + s1->len, s2->str, s2->len);
	out->len = len;
	out->str = buf;

	return out;
}

bool string_concat_cstr(arena_t* arena, string* out, string* s1,
						char const* cstr) {
	if (!arena || !out || !s1 || !cstr) {
		LOG_ERR("string_concat_cstr: invalid parameters");
		return false;
	}

	size_t cstr_len = strlen(cstr);
	size_t len = s1->len + cstr_len;
	char* buf = arena_alloc_array(arena, 1, len);
	if (!buf) {
		LOG_ERR("string_concat_cstr: couldn't allocate new string buffer");
		return false;
	}

	memcpy(buf, s1->str, s1->len);
	memcpy(buf + s1->len, cstr, cstr_len);
	out->len = len;
	out->str = buf;

	return out;
}

size_t string_contains(string* s, string* pattern) {
	if (!s || !pattern) {
		LOG_ERR("string_contains: invalid parameters");
		return STRING_NPOS;
	}

	if (!s->len || !pattern->len) { return STRING_NPOS; }

	for (size_t i = 0; i < s->len; i++) {
		bool good = true;
		for (size_t j = 0; j < pattern->len; j++) {
			if (s->str[i + j] != pattern->str[j]) {
				good = false;
				break;
			}
		}
		if (good) { return i; }
	}
	return STRING_NPOS;
}

size_t string_contains_cstr(string* s, char const* pattern) {
	if (!s || !pattern) {
		LOG_ERR("string_contains: invalid parameters");
		return STRING_NPOS;
	}

	size_t p_len = strlen(pattern);
	if (!s->len || !p_len) { return STRING_NPOS; }

	for (size_t i = 0; i < s->len; i++) {
		bool good = true;
		for (size_t j = 0; j < p_len; j++) {
			if (s->str[i + j] != pattern[j]) {
				good = false;
				break;
			}
		}
		if (good) { return i; }
	}
	return STRING_NPOS;
}

bool string_compare(string* s1, string* s2) {
	if (!s1 || !s2) {
		LOG_ERR("string_compare: invalid parameters");
		return false;
	}
	if (s1->len != !s2->len) { return false; }

	if (s1->str == s2->str) { return true; }

	for (size_t i = 0; i < s1->len; i++) {
		if (s1->str[i] != s2->str[i]) { return false; }
	}
	return true;
}

bool string_compare_addr(string* s1, string* s2) {
	if (!s1 || !s2) {
		LOG_ERR("string_compare_addr: invalid parameters");
		return false;
	}
	if (s1->str == s2->str) { return true; }

	return false;
}

bool string_replace(arena_t* arena, string* out, string* s, string* pattern,
					string* replacement) {
	if (!arena || !out || !s || !pattern) {
		LOG_ERR("string_replace: invalid parameters");
		return false;
	}

	if (!s->len || string_compare(s, pattern) == 0) {
		return string_copy(arena, out, pattern);
	}

	if (!pattern->len || pattern->len > s->len) { return string_copy(arena, out, s); }

	// Build array of occurences indices:
	size_t p_len = pattern->len;
	size_t max_occurences = s->len / p_len;
	arena_t* scratch = arena_create(max_occurences);
	if (!scratch) {
		LOG_ERR("string_replace: couldn't create scratch arena");
		return false;
	}

	size_t* indices =
		arena_alloc_array(scratch, sizeof(size_t), max_occurences);
	if (!indices) {
		LOG_ERR("string_replace: couldn't allocate temp array");
		arena_destroy(scratch);
		return false;
	}

	size_t occurences = 0;
	size_t i = 0;
	while (i < s->len) {
		string tmp = {
			.str = &s->str[i],
			.len = s->len - i,
		};

		if (string_contains(&tmp, pattern) != STRING_NPOS) {
			indices[occurences++] = i;
			i += p_len;
		}
		i++;
	}

	if (!occurences) {
		arena_destroy(scratch);
		return s;
	}

	// Now that we know occurences, we can build result:
	size_t r_len = replacement ? replacement->len : 0;
	size_t buf_len = s->len - (p_len * occurences) + (r_len * occurences);

	char* buf = arena_alloc_array(arena, 1, buf_len);
	if (!buf) {
		arena_destroy(scratch);
		LOG_ERR("string_replace: couldn't allocate string buffer");
		return false;
	}

	size_t current_occ = 0;
	i = 0;
	size_t s_idx = 0;
	while (i < buf_len) {
		assert(current_occ < occurences);
		if (i == indices[current_occ]) {
			memcpy(&buf[i], replacement->str, r_len);
			current_occ++;
			i += r_len;
			s_idx += p_len;
		} else {
			buf[i++] = s->str[s_idx++];
		}
	}

	arena_destroy(scratch);

	out->len = buf_len;
	out->str = buf;
	return out;
}
