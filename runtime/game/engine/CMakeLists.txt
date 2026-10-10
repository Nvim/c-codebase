# == Engine entrypoint == #
add_executable(engine_exe)

# build/{preset}/runtime/game
set_target_properties(
    engine_exe
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_RUNTIME_DIR}"
)

target_include_directories(engine_exe PRIVATE ./include)
target_link_libraries(
    engine_exe
    PRIVATE compile_flags platform core graphics loaders
)

file(
    GLOB_RECURSE ENGINE_SRC
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
target_sources(engine_exe PRIVATE ${ENGINE_SRC})

# == Engine public API, game can include == #
# [NOTE]: although includes are visible, game will only get access to engine
# public functions via DLL "handshake" on startup. No linking with engine.
add_library(engine_api INTERFACE)
target_include_directories(engine_api INTERFACE ./include)

# == Shaders == #
# Adapted from Vulkan Samples: https://github.com/KhronosGroup/Vulkan-Samples
# Licensed under the Apache License, Version 2.0.
file(
    GLOB_RECURSE ENGINE_SHADER_FILES_SLANG
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.slang"
)
set(OUTPUT_FILES "")
set(SLANG_TARGET_NAME ${PROJECT_NAME}-SLANG)
foreach(SHADER_FILE_SLANG ${ENGINE_SHADER_FILES_SLANG})
    get_filename_component(SLANG_SPV_FILE ${SHADER_FILE_SLANG} NAME_WLE)
    get_filename_component(bare_name ${SLANG_SPV_FILE} NAME_WLE)
    get_filename_component(directory ${SHADER_FILE_SLANG} DIRECTORY)
    set(OUTPUT_DIR "${GAME_RUNTIME_DIR}/shaders")
    set(OUTPUT_FILE ${OUTPUT_DIR}/${bare_name}.spv)
    file(MAKE_DIRECTORY ${OUTPUT_DIR})
    set(SLANG_PROFILE "spirv_1_6")
    set(SLANG_VERT_ENTRY_POINT "vertMain")
    set(SLANG_FRAG_ENTRY_POINT "fragMain")
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
		COMMAND cmake -E echo "Compiling Slang shader ${bare_name}.spv"
        COMMAND
            slangc ${SHADER_FILE_SLANG} -profile ${SLANG_PROFILE} -o
            ${OUTPUT_FILE} -entry ${SLANG_VERT_ENTRY_POINT} -entry
            ${SLANG_FRAG_ENTRY_POINT}
        MAIN_DEPENDENCY ${SHADER_FILE_SLANG}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    list(APPEND OUTPUT_FILES ${OUTPUT_FILE})
endforeach()

add_custom_target(${SLANG_TARGET_NAME} DEPENDS ${OUTPUT_FILES})
set_property(TARGET ${SLANG_TARGET_NAME} PROPERTY FOLDER "Shaders-SLANG")
add_dependencies(engine_exe ${SLANG_TARGET_NAME})
