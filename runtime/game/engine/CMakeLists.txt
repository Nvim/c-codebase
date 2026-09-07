# == Engine entrypoint == #
add_executable(engine_exe src/main.c)

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

# == Engine public API, game can include == #
# [NOTE]: although includes are visible, game will only get access to engine
# public functions via DLL "handshake" on startup. No linking with engine.
add_library(engine_api INTERFACE)
target_include_directories(engine_api INTERFACE ./include)
