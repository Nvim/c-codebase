add_library(loaders SHARED)

target_include_directories(loaders PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(
    loaders
    PUBLIC compile_flags core allocators containers strings filesystem
)

target_sources(loaders PRIVATE qoi.c json.c)

# ---- Tests ----
add_executable(json_test json_test.c)
target_link_libraries(
    json_test
    PRIVATE
        compile_flags
        core
        allocators
        containers
        strings
        filesystem
        unity
        loaders
)
add_test(NAME json_test COMMAND json_test)

set_tests_properties(
    json_test
    PROPERTIES WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
