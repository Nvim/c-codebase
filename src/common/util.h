#ifndef UTIL_H
#define UTIL_H

#include <stdarg.h>
#include <stdio.h>

#if !defined(__clang__)
#error "Unsupported compiler"
#endif

#define STATIC_ASSERT _Static_assert

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

#endif // !UTIL_H
