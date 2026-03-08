#ifndef HASHTABLE_TYPES_H
#define HASHTABLE_TYPES_H

#include <containers/hashtable.h>

#include <stdint.h>

HASHTABLE_DEFINE_TYPE(int64_t, i64_hashtable)
HASHTABLE_DEFINE_TYPE(int32_t, i32_hashtable)
HASHTABLE_DEFINE_TYPE(int16_t, i16_hashtable)
HASHTABLE_DEFINE_TYPE(int8_t, i8_hashtable)

HASHTABLE_DEFINE_TYPE(uint64_t, u64_hashtable)
HASHTABLE_DEFINE_TYPE(uint32_t, u32_hashtable)
HASHTABLE_DEFINE_TYPE(uint16_t, u16_hashtable)
HASHTABLE_DEFINE_TYPE(uint8_t, u8_hashtable)

#endif // !HASHTABLE_TYPES_H
