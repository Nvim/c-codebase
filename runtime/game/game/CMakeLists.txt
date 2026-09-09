add_library(game SHARED)

# build/{preset}/runtime/game
set_target_properties(
    game
	PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${GAME_RUNTIME_DIR}"
)

target_link_libraries(game PRIVATE compile_flags engine_api core containers)

file(
	GLOB_RECURSE GAME_SRC
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
target_sources(game PRIVATE ${GAME_SRC})
