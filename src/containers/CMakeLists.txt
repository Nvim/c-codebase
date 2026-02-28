add_library(containers SHARED)

target_include_directories(containers PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Some libs need other libs' internals for hacky things
# e.g string uses array_impl
target_include_directories(containers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link against core
target_link_libraries(containers PUBLIC compile_flags core)

add_subdirectory(arena)
add_subdirectory(string)
add_subdirectory(hashtable)
add_subdirectory(queue)
add_subdirectory(array)
