#include <core/log.h>
#include <filesystem/filesystem.h>
#include <platform/platform.h>

#include <string.h>

string fs_format_memory_size(arena_t* arena, size_t size) {
	string ret = str("ERROR");
	if (!arena) {
		LOG_ERR("invalid parameters");
		return ret;
	}

	static string const units[4] = {str("bytes"), str("KiB"), str("MiB"),
									str("GiB")};
	uint32_t i = 0;
	double s = size;
	for (; i < countof(units); i++) {
		if (s < 1000) { break; }
		s /= 1000;
	}

	if (!string_create_fmt(arena, &ret, "%.2f " STR_FMT, s,
						   STR_FMT_ARGS(units[i]))) {
		LOG_ERR("couldn't create string");
		return str("ERROR");
	}

	return ret;
}

fs_file_read_result fs_read_all_file(arena_t* arena, path pth) {
	fs_file_read_result result = {0};

	if (!arena || !pth.len) {
		LOG_ERR("invalid parameters");
		return result;
	}

	char cpath[PATH_MAX_LEN];
	memcpy(cpath, pth.str, pth.len);
	cpath[pth.len] = 0;

	size_t size = platform_file_size(cpath);

	if (size > 0) {
		u8_array bytes = u8_array_create(arena, size);
		if (!bytes.raw) {
			LOG_ERR("couldn't allocate result buffer");
			return result;
		}

		platform_file_read_result r =
			platform_read_file(cpath, u8_array_begin(bytes), size);
		if (r.ok) {
			result.ok = true;
			result.bytes = bytes;
			result.str = (string){.str = (char*)u8_array_begin(bytes),
								  .len = u8_array_size(bytes)};
		} else {
			LOG_ERR("couldn't read from file");
		}
	}
	return result;
}

fs_file_read_result fs_read_file(arena_t* arena, path pth, size_t size) {
	fs_file_read_result result = {0};

	if (!arena || !pth.len) {
		LOG_ERR("invalid parameters");
		return result;
	}

	char cpath[PATH_MAX_LEN];
	memcpy(cpath, pth.str, pth.len);
	cpath[pth.len] = 0;

	u8_array bytes = u8_array_create(arena, size);
	if (!bytes.raw) {
		LOG_ERR("couldn't allocate buffer");
		return result;
	}

	platform_file_read_result r =
		platform_read_file(cpath, u8_array_begin(bytes), size);
	if (r.ok) {
		result.ok = true;
		result.bytes = bytes;
		result.str = (string){
			.str = (char*)u8_array_begin(bytes),
			.len = u8_array_size(bytes),
		};
	} else {
		LOG_ERR("couldn't read from file");
	}

	return result;
}

bool fs_write_all_file(path pth, u8_array data) {
	if (!pth.len || !data.raw) {
		LOG_ERR("invalid parameters");
		return false;
	}

	char cpath[PATH_MAX_LEN];
	memcpy(cpath, pth.str, pth.len);
	cpath[pth.len] = 0;

	bool ok = platform_write_all_file(cpath, u8_array_begin(data),
									  u8_array_size(data));

	return ok;
}

bool fs_append_file(path pth, u8_array data) {
	if (!pth.len || !data.raw) {
		LOG_ERR("invalid parameters");
		return false;
	}

	char cpath[PATH_MAX_LEN];
	memcpy(cpath, pth.str, pth.len);
	cpath[pth.len] = 0;

	bool ok =
		platform_append_file(cpath, u8_array_begin(data), u8_array_size(data));

	return ok;
}
