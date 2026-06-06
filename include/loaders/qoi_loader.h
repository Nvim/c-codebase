#include <common.h>
#include <core/log.h>
#include <filesystem/filesystem.h>
#include <strings/path.h>

typedef struct image_read_result {
	uint32_t width;
	uint32_t height;
	uint32_t channels;
	bool ok;
	u8_array bytes;
} image_read_result;

LIB_EXPORT image_read_result load_qoi(arena_t* arena, arena_t* scratch,
									  path filename);
