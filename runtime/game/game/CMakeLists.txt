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

# = Game Assets = #
add_custom_target(copy_game_assets ALL
    COMMENT "Copying game assets to ${GAME_RUNTIME_DIR}/game_assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${GAME_RUNTIME_DIR}/game_assets
)

# engine config goes to root output dir
add_custom_target(copy_engine_config ALL
    COMMENT "Copying game assets to ${GAME_RUNTIME_DIR}/game_assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/engine_config.json
        ${GAME_RUNTIME_DIR}/engine_config.json
)

add_dependencies(game copy_game_assets)
add_dependencies(game copy_engine_config)
