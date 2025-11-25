/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/pstring.h"

PascalString16 *pstring_create_pstring16(uint16_t size) {
  return NULL;
}

PascalString16 *pstring_create_pstring16_from_string(char string[]) {
  return NULL;
}

void pstring_destroy_pstring16(PascalString16 *pstring) {
}

void pstring_pstring16_to_string(const PascalString16 *pstring, char *string_out) {
}

void pstring_string_to_pstring16(char string[], PascalString16 *pstring_out) {
}

bool pstring_equal(const PascalString16 *ps1, const PascalString16 *ps2) {
  return false;
}

bool pstring_equal_cstring(const PascalString16 *pstr, const char *cstr) {
  return false;
}

SerializedArray *pstring_create_serialized_array(uint16_t data_size) {
  return NULL;
}

void pstring_destroy_serialized_array(SerializedArray* serialized_array) {
}

void pstring_project_list_on_serialized_array(PascalString16List *pstring16_list,
                                              SerializedArray *serialized_array) {
}

bool pstring_add_pstring16_to_list(PascalString16List *pstring16_list, PascalString16* pstring) {
  return true;
}

uint16_t pstring_get_number_of_pstring16s_in_list(PascalString16List *pstring16_list) {
  return 0;
}

PascalString16* pstring_get_pstring16_from_list(PascalString16List *pstring16_list, uint16_t index) {
  return NULL;
}

void pstring_print_pstring(PascalString16 *pstring) {
}

void pstring_print_pstring16list(PascalString16List *list) {
}
