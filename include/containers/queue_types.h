#ifndef QUEUE_TYPES_H
#define QUEUE_TYPES_H

#include <containers/queue.h>

#include <stdint.h>

QUEUE_DEFINE_TYPE(int64_t, i64_queue)
QUEUE_DEFINE_TYPE(int32_t, i32_queue)
QUEUE_DEFINE_TYPE(int16_t, i16_queue)
QUEUE_DEFINE_TYPE(int8_t, i8_queue)

QUEUE_DEFINE_TYPE(uint64_t, u64_queue)
QUEUE_DEFINE_TYPE(uint32_t, u32_queue)
QUEUE_DEFINE_TYPE(uint16_t, u16_queue)
QUEUE_DEFINE_TYPE(uint8_t, u8_queue)

#endif // !QUEUE_TYPES_H
