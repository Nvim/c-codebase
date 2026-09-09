# == Engine entrypoint == #
add_executable(engine_exe)

# build/{preset}/runtime/game
set_target_properties(
    engine_exe
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_RUNTIME_DIR}"
)

target_include_directories(engine_exe PRIVATE ./include)
target_link_libraries(
    engine_exe
    PRIVATE compile_flags platform core graphics loaders
)

file(
    GLOB_RECURSE ENGINE_SRC
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
target_sources(engine_exe PRIVATE ${ENGINE_SRC})

# == Engine public API, game can include == #
# [NOTE]: although includes are visible, game will only get access to engine
# public functions via DLL "handshake" on startup. No linking with engine.
add_library(engine_api INTERFACE)
target_include_directories(engine_api INTERFACE ./include)
