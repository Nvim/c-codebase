#ifndef PLATFORM_H
#define PLATFORM_H

#include <common.h>
#include <keycodes.h>
#include <visibility.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LIB_EXPORT bool platform_init(void);
LIB_EXPORT void platform_destroy(void);

/**
 * @brief Get initialization info for a graphics API
 *
 * @param type API type to initialize
 * @param out The info struct to fill. Will be casted to according type
 * @return False if platform isn't supported or on graphics API error.
 */
LIB_EXPORT bool platform_init_graphics_api(graphics_api_type type,
										   graphics_api_init_info* out);

/* ********************************[ MEMORY ]******************************** */

LIB_EXPORT static size_t const MEMORY_GIB = 1024UL * 1024UL * 1024UL;
LIB_EXPORT static size_t const MEMORY_MIB = 1024UL * 1024UL;
LIB_EXPORT static size_t const MEMORY_KIB = 1024UL;

/**
 * @brief Queries the OS memory page size
 */
LIB_EXPORT size_t platform_memory_page_size(void);

/**
 * @brief Returns the beginning of address' memory page
 */
LIB_EXPORT void* platform_memory_page_begin(void* addr);

/**
 * @brief Returns the beginning of the next memory page
 */
LIB_EXPORT void* platform_memory_next_page(void* addr);

/**
 * @brief Returns the last byte of current page
 */
LIB_EXPORT void* platform_memory_page_end(void* addr);

/**
 * @brief Simple malloc wrapper.
 *
 * @param size The desired size in bytes
 */
LIB_EXPORT void* platform_memory_alloc(size_t size);

/**
 * @brief Free a memory block allocated by `platform_memory_alloc`.
 */
LIB_EXPORT void platform_memory_free(void* memory);

/**
 * @brief Reserve memory address space.
 *
 * @param size Desired size in bytes (need not be page-aligned).
 * @return Page-aligned base address, or NULL on failure.
 */
LIB_EXPORT void* platform_memory_reserve(size_t size);

/**
 * @brief De-reeserve memory address space.
 *
 * @param memory Page-aligned base address, or NULL on failure.
 * @param size Size of the reserved space in bytes (need not be page-aligned).
 */
LIB_EXPORT bool platform_memory_dereserve(void* memory, size_t size);

/**
 * @brief Commit memory address space, aligned up to OS memory page size.
 *
 * @param memory Beginning of the memory region to commit (must be page-aligned)
 * @param size Amount of memory to commit (must be page-aligned)
 * @return True on success, false on failure
 */
LIB_EXPORT bool platform_memory_commit(void* memory, size_t size);

/**
 * @brief Decommit memory adress space, while keeping it reserved.
 *
 * @param memory Beginning of the memory region to decommit (must be
 * page-aligned).
 * @param size Amount of memory to commit (must be
 * page-aligned)
 * @return True on success, false on failure
 */
LIB_EXPORT bool platform_memory_decommit(void* memory, size_t size);

/* *******************************[ FILE I/O ]******************************* */

// In case I ever need more fields
typedef struct platform_file_read_result {
	bool ok;
} platform_file_read_result;

LIB_EXPORT size_t platform_file_size(char const* path);

LIB_EXPORT platform_file_read_result platform_read_all_file(char const* path,
															uint8_t* buffer);

LIB_EXPORT platform_file_read_result platform_read_file(char const* path,
														uint8_t* buffer,
														size_t read_size);

LIB_EXPORT bool platform_write_all_file(char const* path, uint8_t* data,
										size_t data_size);

LIB_EXPORT bool platform_append_file(char const* path, uint8_t* data,
									 size_t data_size);

/* ********************************[ WINDOW ]******************************** */

/**
 * [NOTE]:
 * Window API is intended to be used with Vulkan. Thus, it doesn't provide
 * a traditionnal "swap_buffers" function. Instead, the expected usage is to
 * get a Vulkan surface from the window, and use the Vulkan swapchain and
 * presenting mechanisms.
 *
 */

typedef struct window_t window_t;

typedef struct window_size {
	int width;
	int height;
} window_size;

LIB_EXPORT window_t* platform_create_window(window_size size, char const* name);

LIB_EXPORT window_size platform_get_window_size(window_t* window);

LIB_EXPORT bool platform_set_window_name(window_t* window, char const* name);

LIB_EXPORT const char* platform_get_window_name(window_t* window);

LIB_EXPORT bool platform_window_should_close(window_t* window);

LIB_EXPORT void platform_destroy_window(window_t* window);

/* ********************************[ INPUT ]********************************* */

LIB_EXPORT void platform_poll(void);

#endif
