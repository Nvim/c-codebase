add_library(loaders SHARED)

target_include_directories(loaders PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(loaders PUBLIC compile_flags core allocators containers strings filesystem)
target_sources(loaders PRIVATE qoi.c)
