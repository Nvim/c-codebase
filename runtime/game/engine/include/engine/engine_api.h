#ifndef ENGINE_FUNCTIONS_H
#define ENGINE_FUNCTIONS_H

#include <strings/string.h>

typedef struct engine_api {
	string (*engine_get_version)(void);
} engine_api;

#endif // !ENGINE_FUNCTIONS_H
