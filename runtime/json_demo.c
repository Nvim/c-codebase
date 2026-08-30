#include "../src/loaders/json_internal.h" // TODO: bad

#include <allocators/arena.h>
#include <loaders/json_loader.h>
#include <strings/string.h>

int main(void) {

	string json_content = str("[ 123, 456, 789, { \"key\": 999 }, {\n} ]");
	arena_t* arena = arena_create(4 * MEMORY_KIB);

	json_parser parser = json_parser_init(arena, json_content, 100);

	json_node* node = json_parser_parse(&parser);
	ASSERT(node);

	ASSERT(node->value_type == JSON_VALUE_ARRAY);
	ASSERT(node->value.array.size == 5);

	json_array_elem* elem = node->value.array.first;

	for (uint32_t i = 0; i < 3; i++) {
		ASSERT(elem->node->value_type == JSON_VALUE_NUMBER);
		LOG_INFO("%f ", elem->node->value.number);
		elem = elem->next;
	}

	ASSERT(elem->node->value_type == JSON_VALUE_OBJECT);
	ASSERT(elem->index == 3);

	ASSERT(elem->node->value.object.size == 1);
	json_object_elem* obj = elem->node->value.object.first;
	ASSERT(obj->next == NULL);
	ASSERT(string_compare(obj->key, str("key")) == true);
	ASSERT(obj->value->value_type == JSON_VALUE_NUMBER);
	ASSERT(obj->value->value.number == 999.f);

	elem = elem->next;
	ASSERT(elem->next == NULL);
	ASSERT(elem->node->value_type == JSON_VALUE_OBJECT);
	ASSERT(elem->node->value.object.size == 0);
	ASSERT(elem->node->value.object.first == NULL);

	return 0;
}
