project(game LANGUAGES C)

# engine executable and libgame.so will both be in
# build/{preset}/runtime/game
set(GAME_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(engine)
add_subdirectory(game)

# == CTest command to run from right working directory == #
add_test(NAME game COMMAND engine_exe)
set_tests_properties(
    game
    PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
