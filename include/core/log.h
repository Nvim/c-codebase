#ifndef UTIL_H
#define UTIL_H

#include "visibility.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define LOG_MAX_LEN 1024

enum log_color {
	COLOR_FATAL = 41,
	COLOR_ERR = 31,
	COLOR_WARN = 33,
	COLOR_INFO = 34,
	COLOR_DEBUG = 35,
};

static char const* log_prefixes[] = {
	[COLOR_FATAL] = "[FATAL]",	 //
	[COLOR_ERR] = "[ERR]: ",	 //
	[COLOR_WARN] = "[WARN]: ",	 //
	[COLOR_INFO] = "[INFO]: ",	 //
	[COLOR_DEBUG] = "[DEBUG]: ", //
};

LIB_EXPORT void log_out(FILE* stream, enum log_color color, char const* msg,
						...);

#define LOG_FATAL(msg, ...)                                                    \
	{ log_out(stderr, COLOR_FATAL, msg, ##__VA_ARGS__); }

#define LOG_ERR(msg, ...)                                                      \
	{ log_out(stderr, COLOR_ERR, msg, ##__VA_ARGS__); }

#define LOG_WARN(msg, ...)                                                     \
	{ log_out(stderr, COLOR_WARN, msg, ##__VA_ARGS__); }

#define LOG_INFO(msg, ...)                                                     \
	{ log_out(stderr, COLOR_INFO, msg, ##__VA_ARGS__); }

#ifdef _DEBUG
#define LOG_DEBUG(msg, ...)                                                    \
	{ log_out(stderr, COLOR_DEBUG, msg, ##__VA_ARGS__); }
#else
#define LOG_DEBUG(msg, ...)
#endif

#endif // !UTIL_H
