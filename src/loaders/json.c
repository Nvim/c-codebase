#include "json_internal.h"

#include <ctype.h>
#include <loaders/json_loader.h>
#include <math.h>
#include <string.h>

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

bool json_peek_token(json_lexer* lexer) {
	if (lexer->has_peeked) { return false; }
	char const* ss = lexer->str;

	// Skip whitespace
	while (isspace(*ss)) {
		if (*ss == '\n' || *ss == '\r') { lexer->cur_line++; }
		ss++;
	}

	json_token* out = &lexer->peeked_token;
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
		if (!json_peek_token(lexer)) { return false; }
	}

	// Consume:
	lexer->has_peeked = false;
	memcpy(out, &lexer->peeked_token, sizeof(lexer->peeked_token));
	lexer->str = &lexer->base_str[out->begin_pos + out->length];
	if (out->type == JSON_TOKEN_STRING) { lexer->str += 2; }

	return true;
}

// Parse
json_parser json_parser_init(arena_t* arena, string str) {
	json_lexer lexer = json_lexer_init(str.str);
	return (json_parser){.arena = arena, .lexer = lexer};
}

bool json_parser_parse(json_parser* parser, json_node* out) {

	json_parser* p = parser;

	ASSERT(p->lexer.base_str == p->lexer.str);
	if (!json_peek_token(&p->lexer)) { return false; }
	json_token* token = &p->lexer.peeked_token;

	switch (token->type) {
	case JSON_TOKEN_EOF:
	case JSON_TOKEN_COMMA:
	case JSON_TOKEN_COLON:
	case JSON_TOKEN_CLOSE_BRACKET:
	case JSON_TOKEN_CLOSE_SQ_BRACKET:
		LOG_ERR("unexpected token %s at position %u",
				json_token_names[token->type], token->begin_pos);
		return false;
	case JSON_TOKEN_OPEN_BRACKET:
		if (!json_parse_array(p, out)) {
			LOG_ERR("error parsing array node at line %u", p->lexer.cur_line);
			return false;
		}
		break;
	case JSON_TOKEN_OPEN_SQ_BRACKET:
		if (!json_parse_object(p, out)) {
			LOG_ERR("error parsing object node at line %u", p->lexer.cur_line);
			return false;
		}
		break;
	case JSON_TOKEN_KEYWORD:
		if (!json_parse_keyword(p, out)) {
			LOG_ERR("error parsing keyword at line %u", p->lexer.cur_line);
			return false;
		}
		break;
	case JSON_TOKEN_STRING:
		if (!json_parse_string(p, out)) {
			LOG_ERR("error parsing string at line %u", p->lexer.cur_line);
			return false;
		}
		break;
	case JSON_TOKEN_NUMBER:
		if (!json_parse_number(p, out)) {
			LOG_ERR("error parsing number at line %u", p->lexer.cur_line);
			return false;
		}
		break;
	}

	// Top-level can only have single node. If anything else is left, error
	json_peek_token(&p->lexer);
	token = &p->lexer.peeked_token;
	if (token->type != JSON_TOKEN_EOF) {
		LOG_ERR("json_parser_parse: unexpected `%s` at line %u (position %u). "
				"Expected EOF",
				json_token_names[token->type], p->lexer.cur_line,
				token->begin_pos);
		return false;
	}

	return true;
}

bool json_parse_keyword(json_parser* parser, json_node* out) {
	json_token token = {0};

	bool r = json_consume_token(&parser->lexer, &token);
	uint64_t line = parser->lexer.cur_line;
	if (!r) {
		LOG_ERR("couldn't consume next token (line %u)", line);
		return false;
	}

	if (token.type != JSON_TOKEN_KEYWORD) {
		LOG_ERR("unexpected token `%s` at (line %u). expected keyword",
				json_token_names[token.type], line);
		return false;
	}

	out->key = STRING_EMPTY;
	if (string_compare(token.value.s, str("null"))) {
		out->value_type = JSON_VALUE_NULL;
	} else if (string_compare(token.value.s, str("true"))) {
		out->value_type = JSON_VALUE_BOOLEAN;
		out->value.boolean = true;
	} else if (string_compare(token.value.s, str("false"))) {
		out->value_type = JSON_VALUE_BOOLEAN;
		out->value.boolean = false;
	} else {
		LOG_ERR("couldn't parse keyword at line %u: " STR_FMT,
				STR_FMT_ARGS(token.value.s));
		return false;
	}

	return true;
}

/**
 * Assumptions from the lexer simplify logic:
 *  - no leading space
 *  - a single (optional) leading minus sign
 *  - no leading zeros in the decimal part
 *  - no leading or trailing '.'
 * [TODO]: exponent notation, enforce length check, don't always assume double
 */
bool json_parse_number(json_parser* parser, json_node* out) {
	json_token token = {0};

	bool r = json_consume_token(&parser->lexer, &token);
	uint64_t line = parser->lexer.cur_line;
	if (!r) {
		LOG_ERR("couldn't consume next token (line %u)", line);
		return false;
	}

	if (token.type != JSON_TOKEN_NUMBER) {
		LOG_ERR("unexpected token `%s` at (line %u). expected number",
				json_token_names[token.type], line);
		return false;
	}

	bool dot_seen = false;
	uint32_t decimal_count = 1; // post-dot digit
	double res = 0.f;
	int sign = 1;

	char* c = token.value.s.str;
	if (*c == '-') {
		sign = -1;
		c++;
	}

	ASSERT(isdigit(*c));
	if (*c - 48 == 0) { ASSERT(*(c + 1) == '.'); }

	while (*c != '\0' && *c != EOF) {
		if (*c == '.') {
			ASSERT(!dot_seen);
			dot_seen = true;
			c++;
			continue;
		}

		if (!isdigit(*c)) { break; }

		double digit = *c - 48;

		if (!dot_seen) {
			res *= 10;
			res += digit;
		} else {
			digit *= 1.f / (pow(10.f, decimal_count++));
			res += digit;
		}

		c++;
	}

	out->key = STRING_EMPTY;
	out->value_type = JSON_VALUE_NUMBER;
	out->value.number = res * sign;

	return true;
}

bool json_parse_string(json_parser* parser, json_node* out) {
	json_token token = {0};

	bool r = json_consume_token(&parser->lexer, &token);
	uint64_t line = parser->lexer.cur_line;
	if (!r) {
		LOG_ERR("couldn't consume next token (line %u)", line);
		return false;
	}

	if (token.type != JSON_TOKEN_STRING) {
		LOG_ERR("unexpected token `%s` at (line %u). expected string",
				json_token_names[token.type], line);
		return false;
	}

	char* c = token.value.s.str;
	if (*c != '"') return false;

	size_t len = 0;
	char const* base = c;

	while (*c != '"') {
		len++;
		c++;
	}

	// [TODO]: allocate and copy it!
	out->value.string.len = len;
	out->value.string.str = (char*)base;

	return true;
}

// TODO
bool json_parse_array(json_parser* parser, json_node* out) {
	(void)parser;
	(void)out;
	return false;
}

// TODO
bool json_parse_object(json_parser* parser, json_node* out) {
	(void)parser;
	(void)out;
	return false;
}
