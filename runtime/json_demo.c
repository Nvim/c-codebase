#include "../src/loaders/json_internal.h" // TODO: bad

#include <allocators/arena.h>
#include <loaders/json_loader.h>
#include <strings/string.h>

int main(void) {

	string json_content = str("[ 123, 456, 789 ]");
	arena_t* arena = arena_create(4 * MEMORY_KIB);

	json_parser parser = json_parser_init(arena, json_content);

	json_node* node = json_parser_parse(&parser);
	ASSERT(node);

	ASSERT(node->value_type == JSON_VALUE_ARRAY);
	ASSERT(node->value.array.size == 3);

	json_array_elem* elem = node->value.array.first;

	for (uint32_t i = 0; i < 3; i++) {
		ASSERT(elem->node->value_type == JSON_VALUE_NUMBER);
		LOG_INFO("%f ", elem->node->value.number);
		elem = elem->next;
	}

	return 0;
}
