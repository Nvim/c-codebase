add_library(graphics SHARED)

target_include_directories(graphics PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(graphics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(graphics PUBLIC compile_flags core allocators containers strings filesystem)

target_sources(graphics PUBLIC graphics.c)

# Vulkan is only option for now
add_subdirectory(vulkan)
