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

json_lexer json_lexer_init(char const* base) {
	return (json_lexer){
		.has_peeked = false,
		.peeked_token = {0},
		.base_str = base,
		.str = base,
		.cur_line = 0,
	};
}

bool json_lex_number(char const* num_str, json_token* out) {

	uint64_t pos = 0;
	bool dot_seen = false;
	bool negative = false;
	bool int_digits = false;
	bool frac_digits = false;

	if (num_str[pos] == '-') {
		negative = true;
		pos++;
	}

	// TODO: exponent notation
	while (isdigit(num_str[pos]) || (!dot_seen && num_str[pos] == '.')) {
		char c = num_str[pos++];

		if (c == '.') {
			if (!int_digits) { return false; }
			dot_seen = true;
			continue;
		}
		if (dot_seen) {
			frac_digits = true;
		} else {
			// first digit can't be a leading 0
			if (!int_digits && c == '0') return false;
			int_digits = true;
		}
	}

	if (pos == 0) { return false; }

	if (dot_seen && !frac_digits) { return false; }
	if (dot_seen && !int_digits) { return false; }

	if (pos == 1) {
		if (negative || dot_seen) { return false; }
	}

	string value = {
		.str = (char*)num_str,
		.len = pos,
	};

	out->type = JSON_TOKEN_NUMBER;
	out->length = value.len;
	out->value.s = value;

	return true;
}

bool json_lex_keyword(char const* s, json_token* out) {
	uint64_t pos = 0;
	char c = s[pos];
	while (isalpha(c)) {
		if (pos > 5) { return false; }
		c = s[pos++];
	}

	string value = {
		.len = pos - 1,
		.str = (char*)s,
	};

	out->length = value.len;
	out->value.s = value;
	out->type = JSON_TOKEN_KEYWORD;

	return true;
}

bool json_lex_string(char const* s, json_token* out) {
	uint64_t pos = 0;
	if (s[pos] != '"') { return false; }
	char c = s[++pos];

	// TODO: escaping
	while (c != '"') { c = s[++pos]; }

	string value = {0};
	if (pos == 1) {
		value = STRING_EMPTY;
	} else {
		value.len = pos - 1;	  // -1 for last quote
		value.str = (char*)s + 1; // +1 skips 1st quote
	};

	out->length = value.len;
	out->value.s = value;
	out->type = JSON_TOKEN_STRING;

	return true;
}

bool json_peek_token(json_lexer* lexer, json_token* out) {
	if (lexer->has_peeked) { return false; }
	char const* ss = lexer->str;

	// Skip whitespace
	while (isspace(*ss)) {
		if (*ss == '\n' || *ss == '\r') { lexer->cur_line++; }
		ss++;
	}

	*out = (json_token){0};

	char chr = *ss;
	switch (chr) {
	case EOF:
	case '\0':
		out->type = JSON_TOKEN_EOF;
		out->begin_pos = ss - lexer->base_str;
		out->value.chr = '\0';
		lexer->has_peeked = true;
		return true;
	case JSON_TOKEN_COMMA:
	case JSON_TOKEN_COLON:
	case JSON_TOKEN_OPEN_BRACKET:
	case JSON_TOKEN_OPEN_SQ_BRACKET:
	case JSON_TOKEN_CLOSE_BRACKET:
	case JSON_TOKEN_CLOSE_SQ_BRACKET: {
		out->type = chr;
		out->value.chr = chr;
		out->begin_pos = ss++ - lexer->base_str;
		out->length = 1;
		lexer->has_peeked = true;
		return true;
	}
	}

	if (isdigit(chr) || chr == '-') { // Number
		if (!json_lex_number(ss, out)) { return false; }
	} else if (isalpha(chr)) { // Keyword
		if (!json_lex_keyword(ss, out)) { return false; }
	} else if (chr == '"') { // String
		if (!json_lex_string(ss, out)) { return false; }
	} else { // Invalid
		return false;
	}

	out->begin_pos = ss - lexer->base_str;
	ss += out->length;
	lexer->has_peeked = true;
	return true;
}

bool json_consume_token(json_lexer* lexer, json_token* out) {
	if (!lexer->has_peeked) {
		// Peek first
		if (!json_peek_token(lexer, &lexer->peeked_token)) { return false; }
	}

	// Consume:
	lexer->has_peeked = false;
	memcpy(out, &lexer->peeked_token, sizeof(lexer->peeked_token));
	lexer->str = &lexer->base_str[out->begin_pos + out->length];
	if (out->type == JSON_TOKEN_STRING) { lexer->str += 2; }

	return true;
}
