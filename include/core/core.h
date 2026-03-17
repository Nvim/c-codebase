#ifndef CORE_H
#define CORE_H

#include <core/log.h>
#include <platform/platform.h>

// [TODO]: support GCC too
#if !defined(__clang__)
#error "Unsupported compiler"
#endif

#define STATIC_ASSERT _Static_assert
#define breakpoint() __builtin_trap()

#ifdef _DEBUG
#define ASSERT(c)                                                              \
	do {                                                                       \
		if (!(c)) {                                                            \
			LOG_FATAL("[ASSERTION FAILED]: '%s' at %s:%d", #c, __FILE__,       \
					  __LINE__);                                               \
			breakpoint();                                                      \
		}                                                                      \
	} while (0)
#else
#define ASSERT(c)                                                              \
	do { (void)(c) } while (0) //
#endif

#endif
