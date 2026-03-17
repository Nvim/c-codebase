#include "containers/queue.h"
#include <assert.h>
#include <common.h>
#include <string.h>
#include <strings/path.h>

#include <allocators/arena.h>
#include <core/log.h>
#include <ctype.h>

bool path_create(arena_t* arena, arena_t* scratch, string str, path* out) {
	if (!arena || !scratch || !out || !str.str || !str.len) {
		LOG_ERR("path_create: invalid parameters");
		return false;
	}

	out->len = 0;
	out->str = NULL;

	size_t begin = 0;
	while (isspace(str.str[begin])) {
		begin++;
		if (begin >= str.len) {
			// Can't be only whitespace
			return false;
		}
	}

	bool absolute = (str.str[begin] == '/');
	if (begin && absolute) {
		// Absolute path: remove leading whitespace
		str.str += begin;
		str.len -= begin;
	}

	// Trim trailing slash if it's the last character
	size_t end = str.len - 1;
	if (str.str[end] == '/' && end != begin) { str.len--; }

	if (str.len == 1) {
		if (!string_copy(arena, out, str)) {
			LOG_ERR("path_create: path copy allocation failed");
			return false;
		}
		return true;
	}

	// We have at least one char, and no tailing /
	string_array slashes;
	if (!string_tokenize(scratch, &slashes, str, str("/"))) {
		LOG_DEBUG("path_create: no slashes in path");
		if (!string_copy(arena, out, str)) {
			LOG_ERR("path_create: path copy allocation failed");
			return false;
		}
		return true;
	}

	queue_t* valid_components = queue_create(
		scratch, string_array_size(slashes) + 1, sizeof(size_t));
	if (!valid_components) {
		LOG_ERR("path_create: couldn't create temp queue");
		return false;
	}

	size_t out_len = absolute ? 1 : 0;

	size_t dot_count = 0;
	for (size_t i = 0; i < string_array_size(slashes); ++i) {
		string* s;
		string_array_at(slashes, &s, i);

		if (s->len == 0) { continue; }
		if (s->len == 1 && s->str[0] == '.') {
			dot_count++;
			continue;
		}
		out_len += s->len + 1;
		queue_enqueue(valid_components, &i);
	}
	if (out_len > 1) { out_len--; }

	out->str = arena_alloc(arena, out_len);
	out->len = out_len;
	ASSERT(out->str);

	if (dot_count == string_array_size(slashes)) {
		out->len = 1;
		out->str[0] = '.';
	}

	size_t pos = 0;
	if (absolute) { out->str[pos++] = '/'; }

	while (!queue_empty(valid_components)) {

		size_t idx;
		queue_dequeue(valid_components, &idx);

		string* s;
		string_array_at(slashes, &s, idx);
		memcpy(out->str + pos, s->str, s->len);
		pos += s->len;
		out->str[pos++] = '/';
	}

	return true;
}

bool path_is_absolute(path p) {
	if (!p.len) { return false; }

	// Absolute paths have no leading whitespace
	return p.str[0] == '/';
}

bool path_get_filename(path p, path* out) {
	if (!p.len) { return false; }

	if (path_is_absolute(p) && p.len == 1) { return false; }

	size_t last_slash = string_contains_last_cstr(p, "/");

	if (last_slash == STRING_NPOS) {
		out->len = p.len;
		out->str = p.str;
		return true;
	}

	if (last_slash >= p.len - 1) {
		// Shouldn't happen, trailing slash is always removed
		LOG_WARN("path_get_filename: path isn't normalized: unexpected "
				 "trailing slash");
		return false;
	}

	out->len = p.len - last_slash - 1;
	out->str = p.str + last_slash + 1;

	return true;
}

bool path_get_extension(path p, string* out) {
	if (!p.len || !out) { return false; }

	path filename;
	if (!path_get_filename(p, &filename)) { return false; }

	size_t last_dot = string_contains_last_cstr(filename, ".");

	if (last_dot == STRING_NPOS) { return false; }

	// Leading dot -> hidden file
	if (last_dot == 0) { return false; }

	// Trailing dot:
	if (last_dot >= filename.len - 1) { return false; }

	out->len = filename.len - last_dot - 1;
	out->str = filename.str + last_dot + 1;

	return true;
}

bool path_get_stem(path p, string* out) {
	if (!p.len || !out) { return false; }

	path filename;
	if (!path_get_filename(p, &filename)) { return false; }

	size_t last_dot = string_contains_last_cstr(filename, ".");

	if (last_dot == STRING_NPOS || last_dot == 0) {
		out->len = filename.len;
		out->str = filename.str;

		return true;
	}

	size_t ext_len = filename.len - last_dot;
	out->len = filename.len - ext_len;
	out->str = filename.str;

	return true;
}

bool path_join(arena_t* arena, arena_t* scratch, path p1, path p2, path* out) {
	if (!arena || !p1.len || !p2.len || !out) {
		LOG_ERR("path_join: invalid parameters");
		return false;
	}

	// Can't join with an absolute path
	if (path_is_absolute(p2)) { return false; }

	if (path_is_absolute(p1) && p1.len == 1) {
		// Single slash, concat p1 and p2 directly:
		return string_concat(arena, (string*)out, p1, p2);
	}

	// Assuming p1 is valid, it doesn't end with trailing / or .
	// First create tmp = p1 + '/', and return tmp + p2
	string tmp;
	if (!string_concat_cstr(scratch, &tmp, p1, "/")) {
		LOG_ERR("path_join: couldn't concat strings");
		return false;
	}
	if (!string_concat(arena, (string*)out, tmp, p2)) {
		LOG_ERR("path_join: couldn't concat strings");
		return false;
	}

	return true;
}

bool path_normalize(path* p, path* out);
