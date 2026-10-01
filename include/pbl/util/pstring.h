/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Length-prefixed string, not NUL-terminated.
struct pbl_pstring16 {
  uint16_t str_length;
  char str_value[];
};

//! Length-prefixed byte array.
struct pbl_serialized_array {
  uint16_t data_size;
  uint8_t data[];
};

//! View of a pbl_serialized_array holding back-to-back pbl_pstring16 entries.
struct pbl_pstring16_list {
  uint16_t count;
  struct pbl_serialized_array *pstrings;
};

//! Copies a pstring16 into a C string.
//! @param pstring String to copy
//! @param string_out Must hold at least pstring->str_length + 1 bytes
void pbl_pstring16_to_cstring(const struct pbl_pstring16 *pstring, char *string_out);

bool pbl_pstring16_equal(const struct pbl_pstring16 *ps1, const struct pbl_pstring16 *ps2);

bool pbl_pstring16_equal_cstring(const struct pbl_pstring16 *pstr, const char *cstr);

//! Points a list at a serialized array and counts its entries. An all-zero array is an empty
//! list, not a list of empty strings.
void pbl_pstring16_list_init(struct pbl_pstring16_list *list,
                             struct pbl_serialized_array *serialized_array);

//! @return the entry at index, or NULL if out of bounds
struct pbl_pstring16 *pbl_pstring16_list_get(const struct pbl_pstring16_list *list, uint16_t index);
