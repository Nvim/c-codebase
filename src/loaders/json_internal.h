#include <allocators/arena.h>
#include <loaders/json_loader.h>

#define MAX_OBJECT_ITEMS 128

typedef enum json_token_type {
	JSON_TOKEN_EOF = 0,
	JSON_TOKEN_COMMA = ',',
	JSON_TOKEN_COLON = ':',
	JSON_TOKEN_OPEN_BRACKET = '{',
	JSON_TOKEN_OPEN_SQ_BRACKET = '[',
	JSON_TOKEN_CLOSE_BRACKET = '}',
	JSON_TOKEN_CLOSE_SQ_BRACKET = ']',
	JSON_TOKEN_KEYWORD = 0x512, // false/true/null
	JSON_TOKEN_STRING,			// key, or string value
	JSON_TOKEN_NUMBER,			// contents of a number
} json_token_type;

typedef struct json_token {
	json_token_type type;
	uint64_t begin_pos;
	uint64_t length;
	union {
		char chr; // JSON_TOKEN_{COMMA,COLON,OPEN_BRACKET,...}
		string s; // JSON_TOKEN_{KEYWORD,STRING,NUMBER}
	} value;
} json_token;

// [TODO] line should start at one
typedef struct json_lexer {
	string base_str;		 // used to compute current offset
	char const* str;		 // current position (advanced by consuming)
	json_token peeked_token; // peeked token
	uint64_t cur_line;		 // incremented at every newline (by peeking)
	bool has_peeked;		 // when true, `consume` returns `peeked_token`
} json_lexer;

json_lexer json_lexer_init(string base);
bool json_lex_number(string str, json_token* out);
bool json_lex_keyword(string str, json_token* out);
bool json_lex_string(string str, json_token* out);

bool json_peek_token(json_lexer* lexer);
bool json_consume_token(json_lexer* lexer, json_token* out);

/* == Parse == */
typedef struct json_parser {
	arena_t* arena;
	arena_checkpoint* checkpoint; // Used to revert arena on parsing error
	json_lexer lexer;
	int64_t depth; // Entering object/array decrements it. Error when hits 0.
} json_parser;

bool json_parse_string(json_parser* parser, json_node* out, json_token* token);
bool json_parse_keyword(json_parser* parser, json_node* out, json_token* token);
bool json_parse_number(json_parser* parser, json_node* out, json_token* token);
bool json_parse_array(json_parser* parser, json_node* out, json_token* token);
bool json_parse_object(json_parser* parser, json_node* out, json_token* token);

json_parser json_parser_init(arena_t* arena, string str, int64_t max_depth);
json_node* json_parser_parse(json_parser* parser);
void json_parser_free(json_parser* parser);

typedef bool (*JSON_PARSE_FN)(json_parser* parser, json_node* out,
							  json_token* token);

static const JSON_PARSE_FN json_token_to_parse_fn[] = {
	[JSON_TOKEN_OPEN_BRACKET] = json_parse_object,
	[JSON_TOKEN_OPEN_SQ_BRACKET] = json_parse_array,
	[JSON_TOKEN_KEYWORD] = json_parse_keyword,
	[JSON_TOKEN_STRING] = json_parse_string,
	[JSON_TOKEN_NUMBER] = json_parse_number,
};
