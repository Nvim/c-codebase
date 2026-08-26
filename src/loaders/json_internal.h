#include <loaders/json_loader.h>

typedef enum json_token_type {
	JSON_TOKEN_WHITESPACE = ' ',
	JSON_TOKEN_QUOTE = '"', // TODO: not needed?
	JSON_TOKEN_COMMA = ',',
	JSON_TOKEN_COLON = ':',
	JSON_TOKEN_OPEN_BRACKET = '{',
	JSON_TOKEN_OPEN_SQ_BRACKET = '[',
	JSON_TOKEN_CLOSE_BRACKET = '}',
	JSON_TOKEN_CLOSE_SQ_BRACKET = ']',
	JSON_TOKEN_CHAR_SEQUENCE = 0x512, // false/true/null
	JSON_TOKEN_QUOTED_SEQUENCE,		  // key, or string value
	JSON_TOKEN_NUMBER,				  // contents of a number
} json_token_type;

typedef struct json_token {
	json_token_type type;
	uint64_t begin_pos;
	uint64_t length;
} json_token;

bool json_read_whitespace(char const** c);

bool json_read_bool(char const** c, bool* out);

bool json_read_null(char const** c);

bool json_read_string(char const** c, string* out);

bool json_read_number(char const** c, double* out);

bool json_read_token(char const** c, json_token_type* out);
