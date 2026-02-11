add_library(unity STATIC
    unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(unity PUBLIC compile_flags)
