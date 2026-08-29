#include <loaders/json_loader.h>

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
	char const* const base_str; // used to compute current offset
	char const* str;			// current position (advanced by consuming)
	json_token peeked_token;	// peeked token
	uint64_t cur_line;			// incremented at every newline (by peeking)
	bool has_peeked;			// when true, `consume` returns `peeked_token`
} json_lexer;

json_lexer json_lexer_init(char const* base);
bool json_lex_number(char const* num_str, json_token* out);
bool json_lex_keyword(char const* s, json_token* out);
bool json_lex_string(char const* s, json_token* out);

bool json_peek_token(json_lexer* lexer);
bool json_consume_token(json_lexer* lexer, json_token* out);

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

/* == Parse == */
typedef struct json_parser {
	arena_t* arena;
	json_lexer lexer;
} json_parser;

bool json_parse_string(json_parser* parser, json_node* out);
bool json_parse_keyword(json_parser* parser, json_node* out);
bool json_parse_number(json_parser* parser, json_node* out);
bool json_parse_array(json_parser* parser, json_node* out);
bool json_parse_object(json_parser* parser, json_node* out);

json_parser json_parser_init(arena_t* arena, string str);
bool json_parser_parse(json_parser* parser, json_node* out);
