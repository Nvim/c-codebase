#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <allocators/arena.h>
#include <containers/array_types.h>
#include <strings/path.h>
#include <visibility.h>

typedef struct platform_file_read_result {
	bool ok;
	u8_array bytes;
} platform_file_read_result;

LIB_EXPORT platform_file_read_result platform_read_all_file(arena_t* arena,
															path pth);
LIB_EXPORT platform_file_read_result platform_read_file(arena_t* arena,
														path pth, size_t size);

LIB_EXPORT bool platform_write_all_file(path pth, u8_array data);
LIB_EXPORT bool platform_append_file(path pth, u8_array data);

#endif // !FILESYSTEM_H
