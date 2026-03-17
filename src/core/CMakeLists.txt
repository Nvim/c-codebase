add_library(core SHARED log.c)
target_include_directories(core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(core PUBLIC compile_flags)

# add_subdirectory(platform)
