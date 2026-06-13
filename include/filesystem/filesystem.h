#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <allocators/arena.h>
#include <containers/array_types.h>
#include <strings/path.h>
#include <visibility.h>

/**
 * @brief Returns formatted version of a size in bytes/KiB/MiB/GiB.
 *
 * @param arena Arena to allocate the resulting string
 * @param size The size to format
 * @return Formatted string
 */
LIB_EXPORT string fs_format_memory_size(arena_t* arena, size_t size);

typedef struct fs_file_read_result {
	bool ok;
	u8_array bytes;
	string str;
} fs_file_read_result;

LIB_EXPORT fs_file_read_result fs_read_all_file(arena_t* arena, path pth);
LIB_EXPORT fs_file_read_result fs_read_file(arena_t* arena, path pth,
											size_t size);

LIB_EXPORT bool fs_write_all_file(path pth, u8_array data);
LIB_EXPORT bool fs_append_file(path pth, u8_array data);

#endif // !FILESYSTEM_H
