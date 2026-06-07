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

/**
 * @brief Load a QOI image from file. Returns a tightly packed array of bytes in
 * RGB or RGBA format.
 *
 * @param arena Arena to allocate the resulting pixel buffer.
 * @param scratch Arena for temporary allocations.
 * @param filename Image path.
 * @param force_4_channels Return an RGBA array even when QOI image has only 3
 * components.
 * @return Info struct with data. Only valid if `.ok` is true.
 */
LIB_EXPORT image_read_result load_qoi(arena_t* arena, arena_t* scratch,
									  path filename, bool force_4_channels);
