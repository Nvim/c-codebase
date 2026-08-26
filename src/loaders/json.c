#include "json_internal.h"

#include <ctype.h>
#include <loaders/json_loader.h>
#include <math.h>
#include <string.h>

bool json_read_whitespace(char const** c) {
	uint32_t count = 0;
	while (isspace(**c)) {
		(*c)++;
		count++;
	}
	return count != 0;
}

bool json_read_bool(char const** c, bool* out) {
	if (strncmp(*c, "false", 5) == 0) {
		*out = false;
		(*c) += 5;
		return true;
	} else if (strncmp(*c, "true", 4) == 0) {
		*out = true;
		(*c) += 4;
		return true;
	}
	return false;
}

bool json_read_null(char const** c) {
	if (strncmp(*c, "null", 4) == 0) {
		(*c) += 4;
		return true;
	}
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
	int sign = 1;

	if (**c == '-') {
		sign = -1;
		(*c)++;
	}

	if (!isdigit(**c)) { return false; }

	// Leading 0 is legal only when no integer part
	if (**c - 48 == 0) {
		if (*(*c + 1) != '.') { return false; }
	}

	while (**c != '\0' && **c != EOF) {
		if (**c == '.') {
			if (dot_seen) { return false; }

			dot_seen = true;
			(*c)++;
			continue;
		}

		if (!isdigit(**c)) { break; }

		double digit = **c - 48;

		if (!dot_seen) {
			res *= 10;
			res += digit;
		} else {
			digit *= 1.f / (pow(10.f, decimal_count));

			res += digit;
			decimal_count++;
		}

		(*c)++;
	}
	*out = res * sign;
	return true;
}

bool json_read_token(char const** c, json_token_type* out) {
	if (isspace(**c)) {
		*out = JSON_TOKEN_WHITESPACE;
		return true;
	}

	switch (**c) {
	case JSON_TOKEN_QUOTE:
	case JSON_TOKEN_COMMA:
	case JSON_TOKEN_COLON:
	case JSON_TOKEN_OPEN_BRACKET:
	case JSON_TOKEN_OPEN_SQ_BRACKET:
	case JSON_TOKEN_CLOSE_BRACKET:
	case JSON_TOKEN_CLOSE_SQ_BRACKET: {
		*out = **c;
		return true;
	}
	}

	if (isdigit(**c)) {
		*out = JSON_TOKEN_NUMBER;
		return true;
	}

	if (isalpha(**c)) {
		*out = JSON_TOKEN_CHAR_SEQUENCE;
		return true;
	}

	return false;
}
