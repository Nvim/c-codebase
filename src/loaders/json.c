#include "json_internal.h"

#include <ctype.h>
#include <loaders/json_loader.h>
#include <string.h>

bool json_read_whitespace(char const** c) {
	while (isspace(**c)) { (*c)++; }
	return true;
}

bool json_read_bool(char const** c, bool* out) {
	if (strncmp(*c, "false", 4) == 0) {
		*out = false;
		return true;
	} else if (strncmp(*c, "true", 4) == 0) {
		*out = true;
		return true;
	} else
		return false;
}

bool json_read_string(char const** c, string* out) {
	if (**c != '"') return false;

	size_t len = 0;
	char const* base = *c;

	while (**c != '"') {
		len++;
		(*c)++;
	}

	// [TODO]: allocate and copy it!
	out->len = len;
	out->str = (char*)base;

	return true;
}

bool json_read_number(char const** c, double* out) {
	bool dot_seen = false;
	uint32_t decimal_count = 1; // post-dot digit
	double res = 0.f;

	while (**c != '\0' && **c != EOF) {
		if (**c == '.') {
			if (dot_seen) { return false; }

			dot_seen = true;
			(*c)++;
			continue;
		}

		if (!isdigit(**c)) { return false; }

		double digit = **c - 48;
		if (!dot_seen) {
			res *= 10;
			res += digit;
		} else {
			digit *= 1.f / (decimal_count * 10);

			res += digit;
			decimal_count++;
		}

		(*c)++;
	}
	*out = res;
	return true;
}
