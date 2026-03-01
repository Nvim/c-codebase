add_library(allocators SHARED)

target_include_directories(allocators PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(allocators PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link against core
target_link_libraries(allocators PUBLIC compile_flags core)

add_subdirectory(arena)
