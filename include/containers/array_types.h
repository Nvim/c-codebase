#ifndef ARRAY_TYPES_H
#define ARRAY_TYPES_H

#include "array.h"

#include <platform/platform.h>
#include <core/core.h>

#include <stdint.h>

ARRAY_DEFINE_TYPE(int64_t, i64_array)
ARRAY_DEFINE_TYPE(int32_t, i32_array)
ARRAY_DEFINE_TYPE(int16_t, i16_array)
ARRAY_DEFINE_TYPE(int8_t, i8_array)

ARRAY_DEFINE_TYPE(uint64_t, u64_array)
ARRAY_DEFINE_TYPE(uint32_t, u32_array)
ARRAY_DEFINE_TYPE(uint16_t, u16_array)
ARRAY_DEFINE_TYPE(uint8_t, u8_array)

#endif // !ARRAY_TYPES_H
