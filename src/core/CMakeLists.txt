add_library(core SHARED)

target_include_directories(core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(core PUBLIC compile_flags)

target_sources(core PRIVATE log.c math.c)
