/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/pstring.h"


#include "stubs_pbl_malloc.h"
#include "stubs_logging.h"


void test_pstring__initialize(void) {
}

void test_pstring__cleanup(void) {
}

void test_pstring__equal(void) {
  const char *ps1_str = "Phil";
  uint8_t ps1_buf[128];
  PascalString16 *ps1 = (PascalString16 *)&ps1_buf;
  ps1->str_length = strlen(ps1_str);
  memcpy(ps1->str_value, ps1_str, strlen(ps1_str));

  const char *ps2_str = "Four";
  uint8_t ps2_buf[128];
  PascalString16 *ps2 = (PascalString16 *)&ps2_buf;
  ps2->str_length = strlen(ps2_str);
  memcpy(ps2->str_value, ps2_str, strlen(ps2_str));

  const char *ps3_str = "PhilG";
  uint8_t ps3_buf[128];
  PascalString16 *ps3 = (PascalString16 *)&ps3_buf;
  ps3->str_length = strlen(ps3_str);
  memcpy(ps3->str_value, ps3_str, strlen(ps3_str));

  const char *ps4_str = "Phil";
  uint8_t ps4_buf[128];
  PascalString16 *ps4 = (PascalString16 *)&ps4_buf;
  ps4->str_length = strlen(ps4_str);
  memcpy(ps4->str_value, ps4_str, strlen(ps4_str));


  cl_assert(pstring_equal(ps1, ps4));
  cl_assert(!pstring_equal(ps1, ps2));
  cl_assert(!pstring_equal(ps1, ps3));
  cl_assert(!pstring_equal(ps2, ps3));
  cl_assert(!pstring_equal(ps1, NULL));
  cl_assert(!pstring_equal(NULL, NULL));
}

void test_pstring__equal_cstring(void) {
  const char *str1 = "Phil";
  uint8_t ps1_buf[128];
  PascalString16 *ps1 = (PascalString16 *)&ps1_buf;
  ps1->str_length = strlen(str1);
  memcpy(ps1->str_value, str1, strlen(str1));


  const char *str2 = "PhilG";


  cl_assert(pstring_equal_cstring(ps1, str1));
  cl_assert(!pstring_equal_cstring(ps1, str2));
  cl_assert(!pstring_equal_cstring(ps1, NULL));
  cl_assert(!pstring_equal_cstring(NULL, NULL));
}
