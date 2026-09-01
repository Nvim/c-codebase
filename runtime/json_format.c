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

	arena_t* fs_arena = arena_create(64 * MEMORY_KIB);
	if (!fs_arena) { return 1; }
	if (!arena_pool_init()) { return 1; }
	scratch_arena scratch;
	if (!arena_get_scratch(NULL, &scratch)) { return 1; }

	string arg_str;
	if (!string_create(fs_arena, &arg_str, argv[1])) {
		LOG_FATAL("couldn't create string from args");
		goto fs_error;
	}

	path filepath;
	if (!path_create(fs_arena, scratch.arena, arg_str, &filepath)) {
		LOG_FATAL("couldn't create path from args");
		goto fs_error;
	}

	fs_file_read_result result = fs_read_all_file(fs_arena, filepath);
	if (!result.ok) {
		LOG_FATAL("couldn't read file " STR_FMT, STR_FMT_ARGS(filepath));
		goto fs_error;
	}

	printf(STR_FMT "\n", STR_FMT_ARGS(result.str));

	arena_t* parse_arena = arena_create(64 * MEMORY_KIB);
	if (!parse_arena) { goto fs_error; }
	json_parser parser = json_parser_init(parse_arena, result.str, 256);
	json_node* root = json_parser_parse(&parser);

	// No need for this anymore, everything is copied to parser's arena
	arena_destroy(fs_arena);
	arena_clear_scratch(scratch);

	if (!root) {
		arena_destroy(parse_arena);
		LOG_FATAL("couldn't parse JSON file");
		return 1;
	}

	print_node(root, 1);

	return 0;

fs_error:
	arena_destroy(fs_arena);
	arena_clear_scratch(scratch);
	return 1;
}
