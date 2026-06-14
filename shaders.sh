#!/bin/sh

set -x

# glslc src/shaders/quad.frag -o src/shaders/quad.frag.spirv
# glslc src/shaders/quad.vert -o src/shaders/quad.vert.spirv

SRC_DIR="src/shaders"
OUT_DIR="build/shaders"

mkdir -p "${OUT_DIR}"

SLANG_OPTS="-profile glsl_450 -target spirv"

slangc "${SRC_DIR}/quad.slang" $SLANG_OPTS -o "${OUT_DIR}/quad.slang.vert.spirv" -entry vertMain -stage vertex
slangc "${SRC_DIR}/quad.slang" $SLANG_OPTS -o "${OUT_DIR}/quad.slang.frag.spirv" -entry fragMain -stage pixel

slangc "${SRC_DIR}/text.slang" $SLANG_OPTS -o "${OUT_DIR}/text.slang.vert.spirv" -entry vertMain -stage vertex
slangc "${SRC_DIR}/text.slang" $SLANG_OPTS -o "${OUT_DIR}/text.slang.frag.spirv" -entry fragMain -stage pixel
