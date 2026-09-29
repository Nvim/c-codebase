#include "json_internal.h"

#include <ctype.h>
#include <filesystem/filesystem.h>
#include <loaders/json_loader.h>
#include <math.h>
#include <string.h>

static char const* json_token_names[] = {
	[JSON_TOKEN_EOF] = "EOF",
	[JSON_TOKEN_COMMA] = "comma",
	[JSON_TOKEN_COLON] = "colon",
	[JSON_TOKEN_OPEN_BRACKET] = "open curly bracket",
	[JSON_TOKEN_OPEN_SQ_BRACKET] = "open square bracket",
	[JSON_TOKEN_CLOSE_BRACKET] = "close curly bracket",
	[JSON_TOKEN_CLOSE_SQ_BRACKET] = "close square bracket",
	[JSON_TOKEN_KEYWORD] = "keyword",
	[JSON_TOKEN_STRING] = "string",
	[JSON_TOKEN_NUMBER] = "number",
};

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
			if (!int_digits && c == '0') {
				if (isdigit(num_str[pos])) { return false; }
			}
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

/**
 * @brief Consume the next token on the stream and return it in `out`.
 * If `json_peek_token` was called before, peeked token will be consumed and
 * returned immediately without additional parsing.
 */
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
json_parser json_parser_init(arena_t* arena, string str, int64_t max_depth) {
	json_lexer lexer = json_lexer_init(str.str);
	arena_checkpoint* check = arena_get_checkpoint(arena);
	return (json_parser){.arena = arena,
						 .lexer = lexer,
						 .checkpoint = check,
						 .depth = max_depth};
}

void json_parser_free(json_parser* parser) {
	if (parser->checkpoint) {
		arena_restore_checkpoint(parser->arena, parser->checkpoint, true);
	}
}

/**
 * @brief Consume token, expect it to be `type`, allocate a new JSON node, and
 * call the specific parsing function for this token. Allocation and error
 * handling is centralized here to keep parsing functions small.
 */
static json_node* _json_parse_node(json_parser* parser, json_token_type type) {
	json_token token = {0};
	bool r = json_consume_token(&parser->lexer, &token);
	uint64_t line = parser->lexer.cur_line;
	if (!r) {
		LOG_ERR("couldn't consume next token (line %u)", line);
		return NULL;
	}

	if (token.type != type) {
		LOG_ERR("unexpected token `%s` at (line %u). expected %s",
				json_token_names[token.type], line, json_token_names[type]);
		return NULL;
	}

	JSON_PARSE_FN parse_func = json_token_to_parse_fn[token.type];
	if (!parse_func) {
		LOG_ERR("unparseable token type `%s`", json_token_names[type]);
		return NULL;
	}

	json_node* result = arena_push_z(parser->arena, json_node);
	if (!result) {
		LOG_ERR("couldn't allocate JSON node");
		return NULL;
	}

	if (!parse_func(parser, result, &token)) {
		LOG_ERR("couldn't parse node for token %s", json_token_names[type]);
		json_parser_free(parser);
		return NULL;
	}

	return result;
}

json_node* json_parser_parse(json_parser* parser) {

	json_parser* p = parser;

	ASSERT(p->lexer.base_str == p->lexer.str);
	if (!json_peek_token(&p->lexer)) { return NULL; }
	json_token* token = &p->lexer.peeked_token;

	json_node* result = NULL;

	switch (token->type) {
	case JSON_TOKEN_EOF:
	case JSON_TOKEN_COMMA:
	case JSON_TOKEN_COLON:
	case JSON_TOKEN_CLOSE_BRACKET:
	case JSON_TOKEN_CLOSE_SQ_BRACKET:
		LOG_ERR("unexpected token %s at position %u",
				json_token_names[token->type], token->begin_pos);
		return NULL;
	case JSON_TOKEN_OPEN_BRACKET:
	case JSON_TOKEN_OPEN_SQ_BRACKET:
	case JSON_TOKEN_KEYWORD:
	case JSON_TOKEN_STRING:
	case JSON_TOKEN_NUMBER: result = _json_parse_node(parser, token->type);
	}

	if (!result) {
		LOG_ERR("error parsing node at line %u (position %u)",
				p->lexer.cur_line, token->begin_pos);
		return NULL;
	}

	// Top-level can only have single node. If anything else is left, error
	json_peek_token(&p->lexer);
	token = &p->lexer.peeked_token;
	if (token->type != JSON_TOKEN_EOF) {
		LOG_ERR("json_parser_parse: unexpected `%s` at line %u (position %u). "
				"Expected EOF",
				json_token_names[token->type], p->lexer.cur_line,
				token->begin_pos);
		json_parser_free(parser);
		return NULL;
	}

	return result;
}

bool json_parse_keyword(UNUSED json_parser* parser, json_node* out,
						json_token* token) {

	if (string_compare(token->value.s, str("null"))) {
		out->value_type = JSON_VALUE_NULL;
	} else if (string_compare(token->value.s, str("true"))) {
		out->value_type = JSON_VALUE_BOOLEAN;
		out->value.boolean = true;
	} else if (string_compare(token->value.s, str("false"))) {
		out->value_type = JSON_VALUE_BOOLEAN;
		out->value.boolean = false;
	} else {
		LOG_ERR("couldn't parse keyword at line %u: " STR_FMT,
				STR_FMT_ARGS(token->value.s));
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
bool json_parse_number(UNUSED json_parser* parser, json_node* out,
					   json_token* token) {

	bool dot_seen = false;
	uint32_t decimal_count = 1; // post-dot digit
	double res = 0.f;
	int sign = 1;

	char* c = token->value.s.str;
	if (*c == '-') {
		sign = -1;
		c++;
	}

	ASSERT(isdigit(*c));
	if (*c - 48 == 0) {
		// must be '0' or '0.xxx'
		if (token->length != 1) {
			if (*(c + 1) != '.') { return false; }
		}
	}

	while (*c != '\0' && *c != EOF) {
		if (*c == '.') {
			if (dot_seen) { return false; }
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

	out->value_type = JSON_VALUE_NUMBER;
	out->value.number = res * sign;

	return true;
}

bool json_parse_string(UNUSED json_parser* parser, json_node* out,
					   json_token* token) {

	char* c = token->value.s.str;

	// [TODO]: process non-ASCII and weird escape sequences (headache)
	while (*c != '"') { c++; }

	// Copy from string to parser's arena
	out->value_type = JSON_VALUE_STRING;
	string_copy(parser->arena, &out->value.string, token->value.s);

	return true;
}

/**
 * @brief Returns true if token can be parsed as a value. i.e:
 * opening brackets, keyword, string or number.
 */
static bool _is_token_value(json_token* token) {
	json_token_type t = token->type;
	if (t == JSON_TOKEN_OPEN_BRACKET || t == JSON_TOKEN_OPEN_SQ_BRACKET)
		return true;

	return (t >= JSON_TOKEN_KEYWORD);
}

static bool _peek_and_expect_value(json_parser* parser) {
	if (!json_peek_token(&parser->lexer)) { return false; }
	if (!_is_token_value(&parser->lexer.peeked_token)) { return false; }
	return true;
}

UNUSED static bool _peek_and_expect(json_parser* parser,
									json_token_type expected) {
	if (!json_peek_token(&parser->lexer)) { return false; }
	json_token* peeked = &parser->lexer.peeked_token;
	if (expected != peeked->type) {
		LOG_ERR("unexpected `%s` token at line %u (position %u)",
				json_token_names[peeked->type], parser->lexer.cur_line,
				peeked->begin_pos);
		return false;
	}
	return true;
}

static bool _consume_and_expect(json_parser* parser, json_token* out,
								json_token_type expected) {
	if (!json_consume_token(&parser->lexer, out)) { return false; }
	if (out->type != expected) {
		LOG_ERR("expected `%s` but got `%s` at line %u (position %u).",
				json_token_names[expected], json_token_names[out->type],
				parser->lexer.cur_line, out->begin_pos);
		return false;
	}
	return true;
}

bool json_parse_array(json_parser* parser, json_node* out, json_token* token) {
	ASSERT(token->type == JSON_TOKEN_OPEN_SQ_BRACKET);

	if (!json_peek_token(&parser->lexer)) { return false; }

	// Empty array:
	if (parser->lexer.peeked_token.type == JSON_TOKEN_CLOSE_SQ_BRACKET) {
		json_token tok;
		if (!json_consume_token(&parser->lexer, &tok)) { return false; }
		out->value_type = JSON_VALUE_ARRAY;
		out->value.array = (json_array){
			.size = 0,
			.first = NULL,
		};
		return true;
	}

	// Depth check:
	parser->depth--;
	if (parser->depth == 0) {
		LOG_ERR("couldn't parse json array: max depth reached");
		return false;
	}

	// Expect and parse a value:
	if (!_is_token_value(&parser->lexer.peeked_token)) { return false; }
	json_node* node = _json_parse_node(parser, parser->lexer.peeked_token.type);
	if (!node) { return false; }

	// Allocate linked-list element for it:
	json_array_elem* prev = arena_push_z(parser->arena, json_array_elem);
	if (!prev) { return false; }
	{
		prev->index = 0;
		prev->node = node;
	}
	json_array_elem* first = prev;

	// Keep going as long as we got commas
	if (!json_peek_token(&parser->lexer)) { return false; }
	while (parser->lexer.peeked_token.type == JSON_TOKEN_COMMA) {
		// Consume comma:
		json_token token;
		if (!json_consume_token(&parser->lexer, &token)) { return false; }

		// Peek and expect value:
		if (!_peek_and_expect_value(parser)) { return false; }

		// Parse the peeked value:
		node = _json_parse_node(parser, parser->lexer.peeked_token.type);
		if (!node) { return false; }

		// Allocate new array element:
		json_array_elem* elem = arena_push_z(parser->arena, json_array_elem);
		if (!elem) { return false; }
		{
			elem->index = prev->index + 1;
			elem->node = node;
			elem->next = NULL;
		}
		prev->next = elem;
		prev = elem;

		// Peek again
		if (!json_peek_token(&parser->lexer)) { return false; }
	}

	// No comma, expect & consume ']'
	json_token t = {0};
	if (!_consume_and_expect(parser, &t, JSON_TOKEN_CLOSE_SQ_BRACKET)) {
		return false;
	}

	parser->depth++;
	{
		out->value_type = JSON_VALUE_ARRAY;
		out->value.array = (json_array){
			.size = prev->index + 1,
			.first = first,
		};
	}
	return true;
}

bool json_parse_object(json_parser* parser, json_node* out, json_token* token) {
	ASSERT(token->type == JSON_TOKEN_OPEN_BRACKET);

	if (!json_peek_token(&parser->lexer)) { return false; }

	// Empty object:
	if (parser->lexer.peeked_token.type == JSON_TOKEN_CLOSE_BRACKET) {
		json_token tok = {0};
		if (!json_consume_token(&parser->lexer, &tok)) { return false; }
		out->value_type = JSON_VALUE_OBJECT;
		out->value.object = (json_object){
			.size = 0,
			.nodes = (json_node_table){0},
		};
		return true;
	}

	// Depth check:
	parser->depth--;
	if (parser->depth == 0) {
		LOG_ERR("couldn't parse json object: max depth reached");
		return false;
	}

	json_node_table map =
		json_node_table_create(parser->arena, MAX_OBJECT_ITEMS, hash_default);
	ASSERT(map.raw);

	uint64_t size = 0;

	while (true) {
		json_token tok = {0};

		// String key, parse it to get node
		json_node* key_node = _json_parse_node(parser, JSON_TOKEN_STRING);
		if (!key_node || key_node->value_type != JSON_VALUE_STRING) {
			return false;
		}
		string key = key_node->value.string;

		// Colon:
		if (!_consume_and_expect(parser, &tok, JSON_TOKEN_COLON)) {
			return false;
		}

		// Peek and expect value:
		if (!_peek_and_expect_value(parser)) { return false; }

		// Parse the peeked value:
		json_node* node =
			_json_parse_node(parser, parser->lexer.peeked_token.type);
		if (!node) { return false; }

		// Allocate new object element:
		json_object_elem* new_elem =
			arena_push_z(parser->arena, json_object_elem);
		if (!new_elem) { return false; }
		{
			new_elem->key = key;
			new_elem->node = node;
		}
		json_node_table_insert(map, key, new_elem);

		size++;

		// If not comma, exit, else consume:
		if (!json_peek_token(&parser->lexer)) { return false; }
		if (parser->lexer.peeked_token.type != JSON_TOKEN_COMMA) { break; }
		if (!json_consume_token(&parser->lexer, &tok)) { return false; }
	}

	// No comma, expect & consume '}'
	json_token t = {0};
	if (!_consume_and_expect(parser, &t, JSON_TOKEN_CLOSE_BRACKET)) {
		return false;
	}

	parser->depth++;

	ASSERT(json_node_table_usage(map) == size);
	{
		out->value_type = JSON_VALUE_OBJECT;
		out->value.object = (json_object){
			.size = size,
			.nodes = map,
		};
	}

	return true;
}

json_node* load_json(arena_t* arena, arena_t* scratch, path filename) {
	if (!arena || !scratch || !filename.len) {
		LOG_ERR("invalid parameters");
		return NULL;
	}

	fs_file_read_result result = fs_read_all_file(scratch, filename);
	if (!result.ok) {
		LOG_ERR("couldn't read file " STR_FMT, STR_FMT_ARGS(filename));
		return NULL;
	}

	json_parser parser = json_parser_init(arena, result.str, 256);
	json_node* root = json_parser_parse(&parser);

	return root;
}
