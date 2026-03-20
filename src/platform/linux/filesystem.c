#include <platform/platform.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static bool _read(int fd, uint8_t* buffer, size_t size) {
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

static bool _write(int fd, uint8_t const* buffer, size_t size) {
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

size_t platform_file_size(char const* path) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) return 0;

	struct stat st;
	if (fstat(fd, &st) != 0) {
		close(fd);
		return 0;
	}

	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return 0;
	}

	return (size_t)st.st_size;
}

platform_file_read_result platform_read_all_file(char const* path,
												 uint8_t* buffer) {
	int fd = open(path, O_RDONLY);
	platform_file_read_result ret = {0};
	if (fd < 0) return ret;

	struct stat st;
	if (fstat(fd, &st) != 0) {
		close(fd);
		return ret;
	}

	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return ret;
	}

	size_t size = (size_t)st.st_size;

	if (size > 0) { ret.ok = _read(fd, buffer, size); }

	close(fd);
	return ret;
}

platform_file_read_result platform_read_file(char const* path, uint8_t* buffer,
											 size_t read_size) {
	platform_file_read_result ret = {0};
	int fd = open(path, O_RDONLY);
	if (fd < 0) return ret;

	ret.ok = _read(fd, buffer, read_size);

	close(fd);
	return ret;
}

bool platform_write_all_file(char const* path, uint8_t* data,
							 size_t data_size) {
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) return false;

	bool ret = _write(fd, data, data_size);

	close(fd);
	return ret;
}

bool platform_append_file(char const* path, uint8_t* data, size_t data_size) {
	int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0) return false;

	bool ret = _write(fd, data, data_size);

	close(fd);
	return ret;
}
