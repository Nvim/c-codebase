#ifndef JSON_LOADER_H
#define JSON_LOADER_H

#include <strings/string.h>

typedef enum json_value_type {
	JSON_VALUE_NUMBER,
	JSON_VALUE_STRING,
	JSON_VALUE_BOOLEAN,
	JSON_VALUE_NULL,
	JSON_VALUE_ARRAY,
	JSON_VALUE_OBJECT,
} json_value_type;

ARRAY_DEFINE_TYPE(json_value_type, json_value_type_array)

typedef struct json_node {
	json_value_type value_type;
	string key; // only present for JSON_VALUE_OBJECT nodes
	union {
		double number;
		string string;
		bool boolean;
		json_value_type_array array;
		struct json_node* object;
	} value;
} json_node;

#endif // !JSON_LOADER_H
