#include "containers/queue.h"
#include <assert.h>
#include <string.h>
#include <strings/path.h>

#include <allocators/arena.h>
#include <core/log.h>
#include <ctype.h>

bool path_create(arena_t* arena, string str, path* out) {
	if (!arena || !out || !str.str || !str.len) {
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
		if (!string_copy(arena, out, &str)) {
			LOG_ERR("path_create: path copy allocation failed");
			return false;
		}
		return true;
	}

	// We have at least one char, and no tailing /
	scratch_arena scratch;
	if (!arena_get_scratch(&scratch)) {
		LOG_ERR("path_create: couldn't get scratch arena");
		return false;
	}

	string s;
	string_create(scratch.arena, &s, "/");
	string_array slashes;
	if (!string_tokenize(scratch.arena, &slashes, &str, &s)) {
		LOG_DEBUG("path_create: no slashes in path");
		if (!string_copy(arena, out, &str)) {
			LOG_ERR("path_create: path copy allocation failed");
			return false;
		}
		arena_clear_scratch(scratch);
		return true;
	}

	queue_t* valid_components = queue_create(
		scratch.arena, string_array_size(slashes) + 1, sizeof(size_t));
	if (!valid_components) {
		LOG_ERR("path_create: couldn't create temp queue");
		arena_clear_scratch(scratch);
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

	arena_clear_scratch(scratch);

	return true;
}
