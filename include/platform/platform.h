#ifndef PLATFORM_H
#define PLATFORM_H

#include <common.h>
#include <keycodes.h>
#include <visibility.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the platform API. Must be called first before using other
 * functions.
 *
 */
LIB_EXPORT bool platform_init(void);

/**
 * @brief Release the platform API resources.
 *
 */
LIB_EXPORT void platform_destroy(void);

typedef struct gpu_api_platform_init_info gpu_api_platform_init_info;
/**
 * @brief Get initialization info for a graphics API
 *
 * @param type API type to initialize
 * @param out The info struct to fill. Will be casted to according type
 * @return False if platform isn't supported or on graphics API error.
 */
LIB_EXPORT bool platform_init_graphics_api(gpu_api_type type,
										   gpu_api_platform_init_info* out);

/**
 * @brief Sleep for the given duration in ms.
 *
 */
LIB_EXPORT void platform_sleep(double time_ms);

/**
 * @brief Get current time in milliseconds
 *
 */
LIB_EXPORT double platform_get_time(void);

/**
 * @brief Get current time in seconds
 *
 */
LIB_EXPORT double platform_get_time_s(void);

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

/**
 * @brief Poll for platform events. Should be called every frame to get
 * mouse/keyboard input and OS window state update.
 *
 */
LIB_EXPORT void platform_poll(void);

/**
 * @note: Only INPUT_ACTION_PRESSED and INPUT_ACTION_RELEASED are supported.
 */
typedef enum key_action {
	INPUT_ACTION_PRESSED = 1 << 0,
	INPUT_ACTION_RELEASED = 1 << 1,
	INPUT_ACTION_HELD = 1 << 2,
	INPUT_ACTION_COUNT = 1 << 3,
} key_action;

typedef void (*key_callback)(window_t* window, input_key key,
							 key_action action);

typedef void (*mouse_button_callback)(window_t* window,
									  input_mouse_button button,
									  key_action action);

/**
 * @brief Register a callback to run for the given key/action combinaison.
 *
 * @param key The keyboard key to register the callback for.
 * @param action The key action(s) to register the callback for. It's possible
 * to stack multiple actions using bitwise ORs
 * @param callback The callback function to run when key/action are hit.
 */
LIB_EXPORT void platform_register_key_callback(input_key key, key_action action,
											   key_callback callback);

/**
 * @brief Register a callback to run for the given mouse button/action
 * combinaison.
 *
 * @param button The mouse button to register the callback for.
 * @param action The key action(s) to register the callback for. It's possible
 * to stack multiple actions using bitwise ORs.
 * @param callback The callback function to run when button/action are hit.
 *
 * @note: Only INPUT_ACTION_PRESSED and INPUT_ACTION_RELEASED are supported.
 */
LIB_EXPORT void
platform_register_mouse_button_callback(input_mouse_button button,
										key_action action,
										mouse_button_callback callback);

#endif
