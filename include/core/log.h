#ifndef UTIL_H
#define UTIL_H

#include "visibility.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define LOG_MAX_LEN 1024

#if defined(__FILE_NAME__)
#define CUR_FILE __FILE_NAME__
#else
#define CUR_FILE __FILE__
#endif

enum log_color {
	COLOR_FATAL = 41,
	COLOR_ERR = 31,
	COLOR_WARN = 33,
	COLOR_INFO = 34,
	COLOR_DEBUG = 35,
};

LIB_EXPORT void log_out(FILE* stream, enum log_color color,
						char const* filename, int linenum, char const* func,
						char const* msg, ...);

#define LOG_FATAL(msg, ...)                                                    \
	{                                                                          \
		log_out(stderr, COLOR_FATAL, CUR_FILE, __LINE__, __func__, msg,        \
				##__VA_ARGS__);                                                \
	}

#define LOG_ERR(msg, ...)                                                      \
	{                                                                          \
		log_out(stderr, COLOR_ERR, CUR_FILE, __LINE__, __func__, msg,          \
				##__VA_ARGS__);                                                \
	}

#define LOG_WARN(msg, ...)                                                     \
	{                                                                          \
		log_out(stderr, COLOR_WARN, CUR_FILE, __LINE__, __func__, msg,         \
				##__VA_ARGS__);                                                \
	}

#define LOG_INFO(msg, ...)                                                     \
	{                                                                          \
		log_out(stderr, COLOR_INFO, CUR_FILE, __LINE__, __func__, msg,         \
				##__VA_ARGS__);                                                \
	}

#ifdef _DEBUG
#define LOG_DEBUG(msg, ...)                                                    \
	{                                                                          \
		log_out(stderr, COLOR_DEBUG, CUR_FILE, __LINE__, __func__, msg,        \
				##__VA_ARGS__);                                                \
	}
#else
#define LOG_DEBUG(msg, ...)
#endif

#endif // !UTIL_H
