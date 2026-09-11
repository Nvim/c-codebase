find_package(Vulkan REQUIRED)

add_library(vma)

set(VMA_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(
    ${PROJECT_SOURCE_DIR}/thirdparty/VulkanMemoryAllocator
    ${PROJECT_SOURCE_DIR}/build/thirdparty/VulkanMemoryAllocator
)

target_link_libraries(vma PRIVATE GPUOpen::VulkanMemoryAllocator)
target_compile_options(vma PRIVATE -fPIC -Wno-nullability-completeness)
target_sources(vma PRIVATE vma.cpp)

target_link_libraries(graphics PUBLIC Vulkan::Vulkan)
target_link_libraries(graphics PRIVATE vma)

target_sources(graphics PRIVATE vulkan.c vulkan_internal.c)
