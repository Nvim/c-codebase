#include "../src/loaders/json_internal.h" // TODO: bad

#include <allocators/arena.h>
#include <loaders/json_loader.h>
#include <strings/string.h>

int main(void) {

	string json_content = str("123.345");
	arena_t* arena = arena_create(4 * MEMORY_KIB);

	json_parser parser = json_parser_init(arena, json_content);

	json_node node;
	ASSERT(json_parser_parse(&parser, &node));

	ASSERT(node.value_type == JSON_VALUE_NUMBER);
	ASSERT(node.value.number == 123.345);

	return 0;
}
