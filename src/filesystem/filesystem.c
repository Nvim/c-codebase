#include <core/filesystem.h>
#include <core/log.h>
#include <core/platform.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool platform_read_all(int fd, uint8_t* buffer, size_t size) {
	size_t total = 0;
	while (total < size) {
		ssize_t r = read(fd, buffer + total, size - total);
		if (r > 0) {
			total += (size_t)r;
			continue;
		}

		if (r == 0) { return false; }
		if (errno == EINTR) { continue; }

		return false;
	}

	return true;
}

static bool platform_write_all(int fd, uint8_t const* buffer, size_t size) {
	size_t total = 0;

	while (total < size) {
		ssize_t w = write(fd, buffer + total, size - total);

		if (w > 0) {
			total += (size_t)w;
			continue;
		}

		if (w < 0 && errno == EINTR) continue;

		return false;
	}

	return true;
}

platform_file_read_result platform_read_all_file(arena_t* arena, path pth) {
	platform_file_read_result result = {0};

	if (!arena || !pth.len) {
		LOG_ERR("platform_read_all_file: invalid parameters");
		return result;
	}

	char cpath[PATH_MAX_LEN];
	memcpy(cpath, pth.str, pth.len);
	cpath[pth.len] = 0;

	int fd = open(cpath, O_RDONLY);
	if (fd < 0) return result;

	struct stat st;
	if (fstat(fd, &st) != 0) {
		close(fd);
		return result;
	}

	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return result;
	}

	size_t size = (size_t)st.st_size;

	if (size > 0) {
		u8_array bytes = u8_array_create(arena, size);
		if (!bytes.raw) {
			LOG_ERR("platform_read_file: couldn't allocate result buffer");
			close(fd);
			return result;
		}

		if (platform_read_all(fd, u8_array_begin(bytes), size)) {
			result.ok = true;
			result.bytes = bytes;
		} else {
			LOG_ERR("platform_read_file: couldn't read from file");
		}
	}

	close(fd);
	return result;
}

platform_file_read_result platform_read_file(arena_t* arena, path pth,
											 size_t size) {
	platform_file_read_result result = {0};

	if (!arena || !pth.len) {
		LOG_ERR("platform_read_all_file: invalid parameters");
		return result;
	}

	char cpath[PATH_MAX_LEN];
	memcpy(cpath, pth.str, pth.len);
	cpath[pth.len] = 0;

	int fd = open(cpath, O_RDONLY);
	if (fd < 0) return result;

	u8_array bytes = u8_array_create(arena, size);
	if (!bytes.raw) {
		LOG_ERR("platform_read_file: couldn't allocate buffer");
		close(fd);
		return result;
	}

	size_t total = 0;

	while (total < size) {
		ssize_t r = read(fd, u8_array_begin(bytes) + total, size - total);

		if (r > 0) {
			total += (size_t)r;
			continue;
		}

		if (r == 0) { break; }
		if (errno == EINTR) { continue; }

		close(fd);
		return result;
	}

	close(fd);

	result.ok = true;
	result.bytes = bytes;

	return result;
}

bool platform_write_all_file(path pth, u8_array data) {
	if (!pth.len || !data.raw) {
		LOG_ERR("platform_write_all_file: invalid parameters");
		return false;
	}

	char cpath[PATH_MAX_LEN];
	memcpy(cpath, pth.str, pth.len);
	cpath[pth.len] = 0;

	int fd = open(cpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) return false;

	bool ok = platform_write_all(fd, u8_array_begin(data), u8_array_size(data));

	close(fd);
	return ok;
}
bool platform_append_file(path pth, u8_array data) {
	if (!pth.len || !data.raw) {
		LOG_ERR("platform_write_all_file: invalid parameters");
		return false;
	}

	char cpath[PATH_MAX_LEN];
	memcpy(cpath, pth.str, pth.len);
	cpath[pth.len] = 0;

	int fd = open(cpath, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0) return false;

	uint8_t* bytes = NULL;
	ASSERT(u8_array_at(data, &bytes, 0));
	bool ok = platform_write_all(fd, bytes, u8_array_size(data));

	close(fd);
	return ok;
}
