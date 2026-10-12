#include <core/log.h>
#include <engine/engine_api.h>
#include <engine/game_api.h>
#include <strings/string.h>

#include <stdbool.h>

string game_get_version(void) { return str("0.0.1"); }

typedef struct game_state {
	arena_t* game_arena;

	engine_api const* engine_ptr;
	uint64_t ticks;
	uint64_t frames;
	vec2 player_pos;
} game_state;

static game_state* _state = NULL;

static bool game_init(engine_api const* engine_api, arena_t* game_arena) {
	LOG_DEBUG("initializing game..");
	UNUSED string engine_version = engine_api->get_version();
	LOG_DEBUG("game intialized with engine API version " STR_FMT,
			  STR_FMT_ARGS(engine_version));

	_state = arena_push_z(game_arena, game_state);
	if (!_state) {
		LOG_ERR("couldn't allocate game state");
		return false;
	}
	_state->game_arena = game_arena;
	_state->engine_ptr = engine_api;
	return true;
}

static void game_process_input(engine_input_state const* input) {
	if (input->pressed_keys[INPUT_KEY_W]) { _state->player_pos.y += 2.f; }
	if (input->pressed_keys[INPUT_KEY_A]) { _state->player_pos.x -= 2.f; }
	if (input->pressed_keys[INPUT_KEY_S]) { _state->player_pos.y -= 2.f; }
	if (input->pressed_keys[INPUT_KEY_D]) { _state->player_pos.x += 2.f; }
}

static bool game_update(double delta) {
	UNUSED static double elapsed = 0.f;
	elapsed += delta;
	// LOG_DEBUG("tick %llu: %f s elapsed. (+%f s)", _state->ticks++, elapsed,
	// delta);
	return true;
}

static bool game_should_quit(void) { return _state->ticks > 100; }

static void game_draw(renderer_packet* out) {
	uint64_t i = 0;
	// for (i = 0; i < MAX_DRAW_ITEMS; i++) {
	renderer_draw_item* player_draw = NULL;
	if (!renderer_draw_item_array_at(out->draw_items, &player_draw, i)) {
		LOG_ERR("can't index draw items array");
		return;
	}
	ASSERT(player_draw);

	player_draw->color = (gpu_color){.r = .8f, .g = 0.f, .b = .9f, .a = 1.f};
	player_draw->position = _state->player_pos;
	player_draw->scale = 1.f;
	out->draw_count++;
	// }
}

static bool game_shutdown(void) {
	LOG_INFO("shutting down game");
	return true;
}

void game_get_functions(game_api* out) {
	*out = (game_api){0};
	out->game_get_version = game_get_version;
	out->game_init = game_init;
	out->game_process_input = game_process_input;
	out->game_should_quit = game_should_quit;
	out->game_update = game_update;
	out->game_draw = game_draw;
	out->game_shutdown = game_shutdown;
}
