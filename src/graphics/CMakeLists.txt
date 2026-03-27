add_library(graphics SHARED)

# Always link with Vulkan SDK, I don't support anything else for now
find_package(Vulkan REQUIRED)

target_include_directories(graphics PUBLIC ${PROJECT_SOURCE_DIR}/include)

# TODO remove this:
target_include_directories(strings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(graphics PUBLIC compile_flags Vulkan::Vulkan core allocators containers strings)

target_sources(graphics PUBLIC vulkan.c)
