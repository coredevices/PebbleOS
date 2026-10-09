/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/util/pstring.h>

#include <clar.h>

void test_pstring__initialize(void) {
}

void test_pstring__cleanup(void) {
}

void test_pstring__equal(void) {
  const char *ps1_str = "Phil";
  uint8_t ps1_buf[128];
  struct pbl_pstring16 *ps1 = (struct pbl_pstring16 *)&ps1_buf;
  ps1->str_length = strlen(ps1_str);
  memcpy(ps1->str_value, ps1_str, strlen(ps1_str));

  const char *ps2_str = "Four";
  uint8_t ps2_buf[128];
  struct pbl_pstring16 *ps2 = (struct pbl_pstring16 *)&ps2_buf;
  ps2->str_length = strlen(ps2_str);
  memcpy(ps2->str_value, ps2_str, strlen(ps2_str));

  const char *ps3_str = "PhilG";
  uint8_t ps3_buf[128];
  struct pbl_pstring16 *ps3 = (struct pbl_pstring16 *)&ps3_buf;
  ps3->str_length = strlen(ps3_str);
  memcpy(ps3->str_value, ps3_str, strlen(ps3_str));

  const char *ps4_str = "Phil";
  uint8_t ps4_buf[128];
  struct pbl_pstring16 *ps4 = (struct pbl_pstring16 *)&ps4_buf;
  ps4->str_length = strlen(ps4_str);
  memcpy(ps4->str_value, ps4_str, strlen(ps4_str));

  cl_assert(pbl_pstring16_equal(ps1, ps4));
  cl_assert(!pbl_pstring16_equal(ps1, ps2));
  cl_assert(!pbl_pstring16_equal(ps1, ps3));
  cl_assert(!pbl_pstring16_equal(ps2, ps3));
  cl_assert(!pbl_pstring16_equal(ps1, nullptr));
  cl_assert(!pbl_pstring16_equal(nullptr, nullptr));
}

void test_pstring__equal_cstring(void) {
  const char *str1 = "Phil";
  uint8_t ps1_buf[128];
  struct pbl_pstring16 *ps1 = (struct pbl_pstring16 *)&ps1_buf;
  ps1->str_length = strlen(str1);
  memcpy(ps1->str_value, str1, strlen(str1));

  const char *str2 = "PhilG";

  cl_assert(pbl_pstring16_equal_cstring(ps1, str1));
  cl_assert(!pbl_pstring16_equal_cstring(ps1, str2));
  cl_assert(!pbl_pstring16_equal_cstring(ps1, nullptr));
  cl_assert(!pbl_pstring16_equal_cstring(nullptr, nullptr));
}

static size_t prv_append(uint8_t *cursor, const char *str, uint16_t length) {
  memcpy(cursor, &length, sizeof(length));
  memset(cursor + sizeof(length), 'x', length);
  if (str) {
    memcpy(cursor + sizeof(length), str, strlen(str));
  }
  return sizeof(length) + length;
}

void test_pstring__list(void) {
  uint8_t buf[64] = {};
  struct pbl_serialized_array *array = (struct pbl_serialized_array *)buf;
  size_t offset = 0;
  offset += prv_append(&array->data[offset], "Palo Alto", 9);
  offset += prv_append(&array->data[offset], "", 0);
  offset += prv_append(&array->data[offset], "Sunny", 5);
  array->data_size = offset;

  struct pbl_pstring16_list list;
  pbl_pstring16_list_init(&list, array);
  cl_assert_equal_i(list.count, 3);
  cl_assert(pbl_pstring16_equal_cstring(pbl_pstring16_list_get(&list, 0), "Palo Alto"));
  cl_assert(pbl_pstring16_equal_cstring(pbl_pstring16_list_get(&list, 1), ""));
  cl_assert(pbl_pstring16_equal_cstring(pbl_pstring16_list_get(&list, 2), "Sunny"));
  cl_assert_equal_p(pbl_pstring16_list_get(&list, 3), nullptr);

  char out[16];
  pbl_pstring16_to_cstring(pbl_pstring16_list_get(&list, 2), out);
  cl_assert_equal_s(out, "Sunny");
}

void test_pstring__list_empty(void) {
  uint8_t buf[16] = {};
  struct pbl_serialized_array *array = (struct pbl_serialized_array *)buf;
  array->data_size = 8;

  struct pbl_pstring16_list list;
  pbl_pstring16_list_init(&list, array);
  cl_assert_equal_i(list.count, 0);
  cl_assert_equal_p(pbl_pstring16_list_get(&list, 0), nullptr);
}

void test_pstring__list_long_entry(void) {
  uint8_t buf[512] = {};
  struct pbl_serialized_array *array = (struct pbl_serialized_array *)buf;
  size_t offset = 0;
  offset += prv_append(&array->data[offset], nullptr, 300);
  offset += prv_append(&array->data[offset], "tail", 4);
  array->data_size = offset;

  struct pbl_pstring16_list list;
  pbl_pstring16_list_init(&list, array);
  cl_assert_equal_i(list.count, 2);
  cl_assert_equal_i(pbl_pstring16_list_get(&list, 0)->str_length, 300);
  cl_assert(pbl_pstring16_equal_cstring(pbl_pstring16_list_get(&list, 1), "tail"));
}
