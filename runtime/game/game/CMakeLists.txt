add_library(game SHARED)

# build/{preset}/runtime/game
set_target_properties(
    game
	PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${GAME_RUNTIME_DIR}"
)

target_link_libraries(game PRIVATE compile_flags engine_api core containers)
target_sources(game PRIVATE game.c)
