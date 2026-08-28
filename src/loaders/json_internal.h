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

typedef struct json_lexer {
	char const* const base_str; // used to compute current offset
	char const* str;
	uint64_t cur_line; // incremented at every newline
} json_lexer;

bool json_read_whitespace(char const** c);

bool json_read_bool(char const** c, bool* out);

bool json_read_null(char const** c);

bool json_read_string(char const** c, string* out);

bool json_read_number(char const** c, double* out);

bool json_lex_number(char const* num_str, json_token* out);
bool json_lex_keyword(char const* s, json_token* out);
bool json_lex_string(char const* s, json_token* out);

bool json_read_token(json_lexer* lexer, json_token* out);
