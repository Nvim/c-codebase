add_library(core SHARED)

target_include_directories(core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(core PUBLIC compile_flags m)

target_sources(core PRIVATE log.c math.c)

# ---- Tests ----
add_executable(math_test math_test.c)
target_link_libraries(math_test PRIVATE compile_flags core unity m)
add_test(NAME math_test COMMAND math_test)

set_tests_properties(
    math_test
    PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
