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

void print_node(json_node* node, uint32_t depth, arena_t* scratch) {
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
			print_node(arr_item->node, depth + 1, scratch);
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
		if (node->value.object.size == 0) {
			printf("}");
			break;
		}

		json_node_table tbl = node->value.object.nodes;
		json_node_table_iterator_t* it = json_node_table_iterate(tbl, scratch);

		bool ok = json_node_table_iterator_next(tbl, it, &obj_item);
		while (ok) {
			printf("\"" STR_FMT "\": ", STR_FMT_ARGS(obj_item->key));
			print_node(obj_item->node, depth + 1, scratch);
			ok = json_node_table_iterator_next(tbl, it, &obj_item);
			if (ok) {
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
		arena_destroy(arena);
		LOG_FATAL("couldn't create string from args");
		return 1;
	}

	json_node* root = load_json(arena, scratch.arena, arg_str);
	ASSERT(root);
	ASSERT(root->value_type = JSON_VALUE_OBJECT);
	arena_clear_scratch(scratch);

	if (!root) {
		arena_destroy(arena);
		LOG_FATAL("couldn't parse JSON file");
		return 1;
	}

	print_node(root, 1, scratch.arena);

	arena_destroy(arena);

	return 0;
}
