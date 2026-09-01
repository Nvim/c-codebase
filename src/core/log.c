#include "visibility.h"
#include <core/log.h>

#include <stdarg.h>
#include <stdio.h>

static char const* log_prefixes[] = {
	[COLOR_FATAL] = "[FATAL]", //
	[COLOR_ERR] = "[ERR]",	   //
	[COLOR_WARN] = "[WARN]",   //
	[COLOR_INFO] = "[INFO]",   //
	[COLOR_DEBUG] = "[DEBUG]", //
};

LIB_EXPORT void log_out(FILE* stream, enum log_color color,
						char const* filename, int linenum, char const* func,
						char const* msg, ...) {
	char log[LOG_MAX_LEN];

	va_list arg_ptr;
	va_start(arg_ptr, msg);
	vsnprintf(log, LOG_MAX_LEN, msg, arg_ptr);
	va_end(arg_ptr);

	fprintf(stream, "\033[;%dm%s(%s:%d) %s: %s\033[0m\n", color,
			log_prefixes[color], filename, linenum, func, log);
}
