add_library(containers SHARED)

target_include_directories(containers PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Link against core
target_link_libraries(containers PUBLIC compile_flags core)

add_subdirectory(arena)
add_subdirectory(string)
add_subdirectory(hashtable)
add_subdirectory(queue)
