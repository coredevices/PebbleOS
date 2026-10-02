/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @defgroup util_string_list String lists
 * @ingroup util
 * @brief Serialized arrays of NUL-separated strings.
 *
 * A string list passes a group of strings as a single attribute on the wire, e.g. the canned
 * responses of a notification. The strings are stored back to back, separated by NUL characters;
 * @ref pbl_string_list::serialized_byte_length does not count the final terminator.
 *
 * @code{.c}
 * uint8_t storage[PBL_STRING_LIST_SIZE(3, 16)] = {0};
 * struct pbl_string_list *list = (struct pbl_string_list *)storage;
 *
 * pbl_string_list_add_string(list, sizeof(storage), "Yes", 16);
 * pbl_string_list_add_string(list, sizeof(storage), "No", 16);
 * const char *second = pbl_string_list_get_at(list, 1); // "No"
 * @endcode
 * @{
 */

/**
 * @brief Get the maximum size of a string list.
 *
 * @param num_values Number of strings.
 * @param max_value_size Maximum size of each string, including its terminator.
 * @return Size in bytes, including the header.
 */
#define PBL_STRING_LIST_SIZE(num_values, max_value_size) \
  (sizeof(struct pbl_string_list) + ((num_values) * (max_value_size)))

/** @brief Serialized string list. */
struct pbl_string_list {
  /** Bytes used in @ref data, excluding the final terminator. 0 for an empty list. */
  uint16_t serialized_byte_length;
  /** NUL-separated strings. */
  char data[];
};

/**
 * @brief Get a string from a string list.
 *
 * @param list String list, may be NULL.
 * @param index Zero-based index of the string.
 * @return Pointer to the string, or NULL if @p list is NULL or @p index is out of bounds.
 */
char *pbl_string_list_get_at(struct pbl_string_list *list, size_t index);

/**
 * @brief Count the strings in a string list.
 *
 * @param list String list, may be NULL.
 * @return Number of strings, 0 for a NULL or empty list.
 */
size_t pbl_string_list_count(struct pbl_string_list *list);

/**
 * @brief Append a string to a string list.
 *
 * The string is truncated to fit the remaining space.
 *
 * @param list String list, may be NULL.
 * @param max_list_size Size of the storage of @p list, including the header and the final
 * terminator.
 * @param str String to add.
 * @param max_str_size Maximum number of characters to read from @p str.
 * @return Number of characters written, excluding the terminator.
 */
int pbl_string_list_add_string(struct pbl_string_list *list, size_t max_list_size, const char *str,
                               size_t max_str_size);

/** @} */
