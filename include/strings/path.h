#ifndef PATH_H
#define PATH_H

#include <strings/string.h>
#include <visibility.h>

#include <stdbool.h>

typedef struct arena_t arena_t;
typedef string path;

/**
 * @brief Create a new path from the given string. Output path is a newly
 * allocated string and doesn't reference original.
 * The path is normalized to follow POSIX specification:
 * - Leading whitespace is only trimmed for absolute paths
 * - Any character besides null byte is allowed
 * - Components are separated by forward slashes
 * - Useless `..` and `.` are removed
 * - Consecutive slashes are removed
 * For convinience, trailing slashes are trimmed. Trailing whitespace is never
 * trimmed and considered valid:
 * - `/home/example   ` -> `/home/example   `
 * - `/home/example/   ` -> `/home/example/   `
 * - `/home/example/` -> `/home/example`
 *
 */
LIB_EXPORT bool path_create(arena_t* arena, string str, path* out);

LIB_EXPORT bool path_is_absolute(path p);

LIB_EXPORT bool path_get_filename(path p, path* out);

LIB_EXPORT bool path_get_extension(path p, string* out);

LIB_EXPORT bool path_get_stem(path p, string* out);

LIB_EXPORT bool path_join(arena_t* arena, path p1, path p2, path* out);

LIB_EXPORT bool path_normalize(path* p, path* out);

#endif // !PATH_H
