#include "engine.h"

#include <loaders/json_loader.h>
#include <strings/string.h>

typedef enum validate_result {
	VALIDATE_ABSENT,	// No elem for key
	VALIDATE_VALID,		// All good
	VALIDATE_BAD_TYPE,	// Elem present but bad type
	VALIDATE_BAD_VALUE, // Elem present but bad value
} validate_result;

validate_result validate_number(json_node_table ht, double* out, string key,
								double min, double max) {
	json_object_elem* elem = json_node_table_get(ht, key);
	if (!elem) { return VALIDATE_ABSENT; }

	if (elem->node->value_type != JSON_VALUE_NUMBER) {
		return VALIDATE_BAD_TYPE;
	}
	double value = elem->node->value.number;
	if (value < min || value > max) {
		LOG_ERR("%f isn't between %f and %f", value, min, max);
		return VALIDATE_BAD_VALUE;
	}
	*out = value;
	return VALIDATE_VALID;
}

validate_result validate_string(json_node_table ht, string* out, string key,
								uint64_t min_len) {
	json_object_elem* elem = json_node_table_get(ht, key);
	if (!elem) { return VALIDATE_ABSENT; }

	if (elem->node->value_type != JSON_VALUE_STRING) {
		return VALIDATE_BAD_TYPE;
	}
	if (elem->node->value.string.len < min_len) { return VALIDATE_BAD_VALUE; }

	*out = elem->node->value.string;

	return VALIDATE_VALID;
}

/**
 * [NOTE] Missing key isn't an error. Unexpected type/value is.
 */
bool _parse_platform(engine_config* out, json_node* node) {
	if (!out || !node || node->value_type != JSON_VALUE_OBJECT) {
		return false;
	}
	json_object o = node->value.object;
	if (!o.size) { return true; }

	double value = 0;
	validate_result v =
		validate_number(o.nodes, &value, str("window_height"), 1.f, 6000.f);
	if (v == VALIDATE_VALID) {
		out->window_size.height = value;
	} else if (v > VALIDATE_VALID) {
		return false;
	}

	v = validate_number(o.nodes, &value, str("window_width"), 1.f, 6000.f);
	if (v == VALIDATE_VALID) {
		out->window_size.width = value;
	} else if (v > VALIDATE_VALID) {
		return false;
	}

	string s = {0};
	v = validate_string(o.nodes, &s, str("window_title"), 1);
	if (v == VALIDATE_VALID) {
		out->window_title = s;
	} else if (v > VALIDATE_VALID) {
		return false;
	}

	return true;
}

bool _parse_memory(engine_config* out, json_node* node) {
	if (!out || !node || node->value_type != JSON_VALUE_OBJECT) {
		return false;
	}
	json_object o = node->value.object;
	if (!o.size) { return true; }

	double value = 0;
	double max_mem = 64 * MEMORY_GIB;

	validate_result v =
		validate_number(o.nodes, &value, str("max_engine_bytes"), 8.f, max_mem);
	if (v == VALIDATE_VALID) {
		out->max_engine_memory_bytes = value;
	} else if (v > VALIDATE_VALID) {
		return false;
	}

	v = validate_number(o.nodes, &value, str("max_game_bytes"), 8.f, max_mem);
	if (v == VALIDATE_VALID) {
		out->max_game_memory_bytes = value;
	} else if (v > VALIDATE_VALID) {
		return false;
	}

	v = validate_number(o.nodes, &value, str("max_frame_bytes"), 8.f, max_mem);
	if (v == VALIDATE_VALID) {
		out->max_frame_memory_bytes = value;
	} else if (v > VALIDATE_VALID) {
		return false;
	}

	return true;
}

bool _parse_game(engine_config* out, json_node* node) {
	if (!out || !node || node->value_type != JSON_VALUE_OBJECT) {
		return false;
	}

	json_object o = node->value.object;
	if (!o.size) { return true; }

	double value = 0;

	validate_result v =
		validate_number(o.nodes, &value, str("resolution_width"), 1.f, 6000.f);
	if (v == VALIDATE_VALID) {
		out->game_width = value;
	} else if (v > VALIDATE_VALID) {
		LOG_ERR("invalid width");
		return false;
	}

	v = validate_number(o.nodes, &value, str("resolution_height"), 1.f, 6000.f);
	if (v == VALIDATE_VALID) {
		out->game_height = value;
	} else if (v > VALIDATE_VALID) {
		LOG_ERR("invalid height");
		return false;
	}

	v = validate_number(o.nodes, &value, str("timestep_seconds"), 1, 360);
	if (v == VALIDATE_VALID) {
		out->game_timestep_s = 1.f / value;
	} else if (v > VALIDATE_VALID) {
		LOG_ERR("invalid timestep");
		return false;
	}

	// TODO: target frametime instead of vsync

	return true;
}

bool engine_config_parse(engine_config* out, json_node* root) {

	if (!out || !root || root->value_type != JSON_VALUE_OBJECT) {
		return false;
	}

	json_object o = root->value.object;
	if (!o.size) { return false; }

	json_object_elem* pf_elem = json_node_table_get(o.nodes, str("platform"));
	ASSERT(pf_elem);
	if (pf_elem) {
		if (!_parse_platform(out, pf_elem->node)) {
			LOG_ERR("couldn't parse config node 'platform'");
			return false;
		}
	}

	json_object_elem* mem_elem = json_node_table_get(o.nodes, str("memory"));
	ASSERT(mem_elem);
	if (mem_elem) {
		if (!_parse_memory(out, mem_elem->node)) {
			LOG_ERR("couldn't parse config node 'memory'");
			return false;
		}
	}

	json_object_elem* game_elem = json_node_table_get(o.nodes, str("game"));
	ASSERT(game_elem);
	if (game_elem) {
		if (!_parse_game(out, game_elem->node)) {
			LOG_ERR("couldn't parse config node 'game'");
			return false;
		}
	}

	return true;
}
