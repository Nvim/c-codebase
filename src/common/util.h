#ifndef UTIL_H
#define UTIL_H

#include <stdarg.h>
#include <stdio.h>

#define LOG_MAX_LEN 1024

void log_out(FILE* stream, char const* msg, ...);

#define LOG_ERR(msg, ...)                                                      \
	{                                                                          \
		fprintf(stderr, "[ERROR]: ");                                          \
		log_out(stderr, msg, ##__VA_ARGS__);                                   \
	}

#define LOG_INFO(msg, ...)                                                     \
	{                                                                          \
		fprintf(stderr, "[INFO]: ");                                           \
		log_out(stderr, msg, ##__VA_ARGS__);                                   \
	}

#ifdef _DEBUG
#define LOG_DEBUG(msg, ...)                                                    \
	{                                                                          \
		fprintf(stderr, "[DEBUG]: ");                                          \
		log_out(stderr, msg, ##__VA_ARGS__);                                   \
	}
#else
#define LOG_DEBUG(msg, ...)
#endif

/* @brief: rounds value up to next alignment multiple */
static inline size_t align_up(size_t value, size_t alignment) {
	size_t idk = (alignment - 1);
	size_t res;
	int over = __builtin_uaddl_overflow(idk, value, &res);
	if (over == 0) { return res & ~(alignment - 1); }
	return 0;
}

#endif // !UTIL_H
