#ifndef JSON_LOADER_H
#define JSON_LOADER_H

#include <strings/path.h>
#include <strings/string.h>

typedef enum json_value_type {
	JSON_VALUE_NUMBER,
	JSON_VALUE_STRING,
	JSON_VALUE_BOOLEAN,
	JSON_VALUE_NULL,
	JSON_VALUE_ARRAY,
	JSON_VALUE_OBJECT,
} json_value_type;

typedef struct json_node json_node;
ARRAY_DEFINE_TYPE(json_value_type, json_value_type_array)

typedef struct json_array_elem {
	uint64_t index;
	json_node* node;
	struct json_array_elem* next;
} json_array_elem;

typedef struct json_array {
	uint64_t size;
	json_array_elem* first;
} json_array;

typedef struct json_object_elem {
	string key;
	json_node* value;
	struct json_object_elem* next;
} json_object_elem;

typedef struct json_object {
	uint64_t size;
	json_object_elem* first;
} json_object;

typedef struct json_node {
	json_value_type value_type;
	union {
		double number;
		string string;
		bool boolean;
		json_array array;
		json_object object;
	} value;
} json_node;

LIB_EXPORT json_node* load_json(arena_t* arena, arena_t* scratch,
								path filename);

#endif // !JSON_LOADER_H
