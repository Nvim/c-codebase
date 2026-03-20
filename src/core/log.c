#include "visibility.h"
#include <core/log.h>

#include <stdarg.h>
#include <stdio.h>

LIB_EXPORT void log_out(FILE* stream, enum log_color color, char const* msg,
						...) {
	char log[LOG_MAX_LEN];

	va_list arg_ptr;
	va_start(arg_ptr, msg);
	vsnprintf(log, LOG_MAX_LEN, msg, arg_ptr);
	va_end(arg_ptr);

	fprintf(stream, "\033[;%dm%s%s\033[0m\n", color, log_prefixes[color], log);
}
