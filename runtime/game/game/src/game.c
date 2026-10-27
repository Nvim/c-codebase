#include <core/log.h>
#include <engine/engine_api.h>
#include <engine/game_api.h>
#include <strings/string.h>

#include <stdbool.h>

string game_get_version(void) { return str("0.0.1"); }

static gpu_color const player_color = {.floats = {.8f, 0.f, .9f, 1.f}};
static gpu_color const BLUE = {.floats = {0.f, 0.f, 1.f, 1.f}};
static gpu_color const RED = {.floats = {1.f, 0.f, 0.f, 1.f}};
static UNUSED gpu_color const GREEN = {.floats = {0.f, 1.f, 0.f, 1.f}};

static vec2 const size_small = {{1.f, 1.f}};
static vec2 const size_wide = {{12.f, 4.f}};
static vec2 const size_big = {{160.f, 90.f}};
static vec2 const player_sizes[] = {size_small, size_wide, size_big};

typedef struct game_state {
	arena_t* game_arena;

	engine_api const* engine_ptr;
	uint64_t ticks;
	uint64_t frames;
	vec2 player_pos;
	uint32_t player_size_idx;
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
	_state->player_size_idx = 0;
	return true;
}

static void game_process_input(engine_input_state const* input) {
	if (input->pressed_keys[INPUT_KEY_W]) { _state->player_pos.y += 2.f; }
	if (input->pressed_keys[INPUT_KEY_A]) { _state->player_pos.x -= 2.f; }
	if (input->pressed_keys[INPUT_KEY_S]) { _state->player_pos.y -= 2.f; }
	if (input->pressed_keys[INPUT_KEY_D]) { _state->player_pos.x += 2.f; }
	if (input->released_keys[INPUT_KEY_SPACE]) {
		_state->player_size_idx =
			(_state->player_size_idx + 1) % countof(player_sizes);
	}
}

static bool game_update(double delta) {
	UNUSED static double elapsed = 0.f;
	elapsed += delta;
	_state->ticks++;
	// LOG_DEBUG("tick %llu: %f s elapsed. (+%f s)", _state->ticks, elapsed,
	// delta);
	return true;
}

static bool game_should_quit(void) { return _state->ticks > 100000; }

static void game_draw(renderer_packet* out) {
	uint64_t i = 0;
	// for (i = 0; i < MAX_DRAW_ITEMS; i++) {
	renderer_draw_item* player_draw = NULL;
	if (!renderer_draw_item_array_at(out->draw_items, &player_draw, i)) {
		LOG_ERR("can't index draw items array");
		return;
	}
	ASSERT(player_draw);

	player_draw->color = player_color;
	player_draw->position = _state->player_pos;
	player_draw->scale = player_sizes[_state->player_size_idx];
	out->draw_count++;
	// }

	// corners
	static vec2 const corners[] = {
		(vec2){.x = -160.f, .y = -90.f},
		(vec2){.x = -160.f, .y = 90.f},
		(vec2){.x = 160.f, .y = -90.f},
		(vec2){.x = 160.f, .y = 90.f},
	};
	for (i = 1; i < 5; i++) {
		renderer_draw_item* draw = NULL;
		ASSERT(renderer_draw_item_array_at(out->draw_items, &draw, i));
		draw->color = player_color;
		draw->position = corners[i - 1];
		draw->scale = (vec2){{4.f, 4.f}};
		out->draw_count++;
	}

	// x axis
	for (int32_t x = -158; x <= 158; x += 2) {
		renderer_draw_item* draw = NULL;
		ASSERT(renderer_draw_item_array_at(out->draw_items, &draw, i++));
		draw->color = BLUE;
		draw->position = (vec2){.x = x, .y = 0.f};
		draw->scale = (vec2){{1.f, 1.f}};
		out->draw_count++;
	}

	// y axis
	for (int32_t y = -88; y <= 88; y += 2) {
		renderer_draw_item* draw = NULL;
		ASSERT(renderer_draw_item_array_at(out->draw_items, &draw, i++));
		draw->color = RED;
		draw->position = (vec2){.x = 0, .y = y};
		draw->scale = (vec2){{1.f, 1.f}};
		out->draw_count++;
	}

	_state->frames++;
}

game_state_ptr game_before_reload(void) { return _state; }

bool game_reload(game_state_ptr state) {
	_state = state;
	LOG_INFO("reloaded game from state %p at tick %ld (frame %ld)", state,
			 _state->frames, _state->ticks);
	return true;
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
	out->game_before_reload = game_before_reload;
	out->game_reload = game_reload;
	out->game_shutdown = game_shutdown;
}
