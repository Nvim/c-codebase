#include <loaders/json_loader.h>

typedef enum json_token_type {
	JSON_TOKEN_WHITESPACE,
	JSON_TOKEN_QUOTE,
	JSON_TOKEN_COMMA,
	JSON_TOKEN_OPEN_BRACKET,
	JSON_TOKEN_OPEN_SQ_BRACKET,
	JSON_TOKEN_CLOSE_BRACKET,
	JSON_TOKEN_CLOSE_SQ_BRACKET,
	JSON_TOKEN_BOOL_VALUE,	 // true/false
	JSON_TOKEN_STRING_VALUE, // contents of a string
	JSON_TOKEN_NUMBER_VALUE, // contents of a number
} json_token_type;

bool json_read_whitespace(char const** c);

bool json_read_bool(char const** c, bool* out);

bool json_read_string(char const** c, string* out);

bool json_read_number(char const** c, double* out);
