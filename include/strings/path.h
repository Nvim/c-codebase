#ifndef PATH_H
#define PATH_H

#include <visibility.h>

#include <stdbool.h>

typedef struct string string;
typedef string path;


LIB_EXPORT bool path_create(string str, path* out);

LIB_EXPORT bool path_is_absolute(path p);

LIB_EXPORT bool path_get_filename(path p, path* out);

LIB_EXPORT bool path_get_extension(path p, string* out);

LIB_EXPORT bool path_get_stem(path p, string* out);

LIB_EXPORT bool path_join(path p1, path p2, path* out);

LIB_EXPORT bool path_normalize(path* p, path* out);



#endif // !PATH_H
