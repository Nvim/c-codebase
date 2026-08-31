#include "../src/loaders/json_internal.h" // TODO: bad

#include <allocators/arena.h>
#include <filesystem/filesystem.h>
#include <loaders/json_loader.h>
#include <platform/platform.h>
#include <strings/path.h>
#include <strings/string.h>

#include <loaders/json_loader.h>

void print_newline(uint32_t count) {
	printf("\n");
	for (uint32_t i = 0; i < count; ++i) { printf("  "); }
}

void print_node(json_node* node, uint32_t depth) {
	json_array_elem* arr_item;
	json_object_elem* obj_item;
	switch (node->value_type) {
	case JSON_VALUE_NUMBER:
		printf("%f", node->value.number); //
		break;
	case JSON_VALUE_STRING:
		printf("\"" STR_FMT "\"", STR_FMT_ARGS(node->value.string));
		break;
	case JSON_VALUE_BOOLEAN:
		if (node->value.boolean) {
			printf("true");
		} else {
			printf("false");
		}
		break;
	case JSON_VALUE_NULL:
		printf("null"); //
		break;
	case JSON_VALUE_ARRAY:
		printf("[");
		print_newline(depth);
		arr_item = node->value.array.first;
		while (arr_item) {
			print_node(arr_item->node, depth + 1);
			arr_item = arr_item->next;
			if (arr_item) {
				printf(",");
				print_newline(depth);
			} else {
				print_newline(depth - 1);
			}
		}
		printf("]");
		break;
	case JSON_VALUE_OBJECT:
		printf("{");
		print_newline(depth);
		obj_item = node->value.object.first;
		while (obj_item) {
			printf("\"" STR_FMT "\": ", STR_FMT_ARGS(obj_item->key));
			print_node(obj_item->value, depth + 1);
			obj_item = obj_item->next;
			if (obj_item) {
				printf(",");
				print_newline(depth);
			} else {
				print_newline(depth - 1);
			}
		}
		printf("}");
		break;
	}
}

int main(int argc, char** argv) {

	if (argc != 2) {
		fprintf(stderr, "usage: %s FILE", argv[0]);
		return 1;
	}

	arena_t* arena = arena_create(64 * MEMORY_KIB);
	if (!arena) { return 1; }
	if (!arena_pool_init()) { return 1; }
	scratch_arena scratch;
	if (!arena_get_scratch(NULL, &scratch)) { return 1; }

	string arg_str;
	if (!string_create(arena, &arg_str, argv[1])) {
		LOG_ERR("couldn't create string from args");
		goto error;
	}

	path filepath;
	if (!path_create(arena, scratch.arena, arg_str, &filepath)) {
		LOG_ERR("couldn't create path from args");
		goto error;
	}

	fs_file_read_result result = fs_read_all_file(arena, filepath);
	if (!result.ok) {
		LOG_ERR("couldn't read file " STR_FMT, STR_FMT_ARGS(filepath));
		goto error;
	}

	printf(STR_FMT "\n", STR_FMT_ARGS(result.str));

	json_parser parser = json_parser_init(arena, result.str, 256);
	json_node* root = json_parser_parse(&parser);
	if (!root) { goto error; }

	print_node(root, 1);

	return 0;

error:
	arena_destroy(arena);
	arena_clear_scratch(scratch);
	return 1;
}
