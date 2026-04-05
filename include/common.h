/**
 * @file
 * @brief Common defines, macros and typedefs that are platform independant
 */

#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

#define BIT(n) 1 << n

inline size_t next_power_of_two(size_t n) {
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;

	return n++;
}

/**
 * @brief Returns the number of elements in an array
 */
#define countof(arr) (sizeof(arr) / sizeof(*(arr)))

/**
 * @brief: rounds value up to next alignment multiple
 * @note alignment must be a non-zero power of two.
 * @return Aligned value, or 0 on overflow
 */
inline size_t align_up(size_t value, size_t alignment) {
	// Alignment must be a power of two. Clamp it otherwise
	if ((alignment & (alignment - 1)) != 0) {
		alignment = next_power_of_two(alignment);
	}

	size_t res;
	return __builtin_uaddl_overflow(alignment - 1, value, &res) == 0
			   ? res & ~(alignment - 1)
			   : 0;
}

#define sec_to_ns(seconds) seconds * 1000000000ULL

typedef enum gpu_api_type {
	GRAPHICS_API_VULKAN,
} gpu_api_type;

#endif // !COMMON_H
