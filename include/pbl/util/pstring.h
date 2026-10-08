/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_pstring Pascal strings
 * @ingroup util
 * @brief Length-prefixed strings and lists of them.
 * @{
 */

/** @brief Length-prefixed string, not NUL-terminated. */
struct PBL_PACKED pbl_pstring16 {
  /** Length of @ref str_value in bytes. */
  uint16_t str_length;
  /** Characters. */
  char str_value[];
};

/** @brief Length-prefixed byte array. */
struct PBL_PACKED pbl_serialized_array {
  /** Size of @ref data in bytes. */
  uint16_t data_size;
  /** Bytes. */
  uint8_t data[];
};

/** @brief View of a pbl_serialized_array holding back-to-back pbl_pstring16 entries. */
struct pbl_pstring16_list {
  /** Number of entries. */
  uint16_t count;
  /** Serialized entries. */
  struct pbl_serialized_array *pstrings;
};

/**
 * @brief Copy a pstring16 into a C string.
 *
 * @param pstring String to copy.
 * @param[out] string_out Destination, at least @c pstring->str_length + 1 bytes.
 */
void pbl_pstring16_to_cstring(const struct pbl_pstring16 *pstring, char *string_out);

/**
 * @brief Compare two pstring16.
 *
 * @param ps1 First string, may be NULL.
 * @param ps2 Second string, may be NULL.
 * @return true if both are non-NULL and equal.
 */
bool pbl_pstring16_equal(const struct pbl_pstring16 *ps1, const struct pbl_pstring16 *ps2);

/**
 * @brief Compare a pstring16 with a C string.
 *
 * @param pstr Length-prefixed string, may be NULL.
 * @param cstr C string, may be NULL.
 * @return true if both are non-NULL and equal.
 */
bool pbl_pstring16_equal_cstring(const struct pbl_pstring16 *pstr, const char *cstr);

/**
 * @brief Point a list at a serialized array and count its entries.
 *
 * An all-zero array is an empty list, not a list of empty strings.
 *
 * @param[out] list List.
 * @param serialized_array Serialized entries, must outlive @p list.
 */
void pbl_pstring16_list_init(struct pbl_pstring16_list *list,
                             struct pbl_serialized_array *serialized_array);

/**
 * @brief Get an entry of a list.
 *
 * @param list List.
 * @param index Zero-based index.
 * @return Entry, or NULL if @p index is out of bounds.
 */
struct pbl_pstring16 *pbl_pstring16_list_get(const struct pbl_pstring16_list *list, uint16_t index);

/** @} */
