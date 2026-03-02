add_library(strings SHARED)

target_include_directories(strings PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(strings PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(strings PRIVATE ${PROJECT_SOURCE_DIR}/src/containers/array) # hacks

target_link_libraries(strings PUBLIC compile_flags core allocators containers)

add_subdirectory(string)
add_subdirectory(path)
