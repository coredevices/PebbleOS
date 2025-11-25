/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clar.h"

#include "stubs_passert.h"


// Stubs
///////////////////////////////////////////////////////////
int g_pbl_log_level = 0;
void pbl_log(const char* src_filename, int src_line_number, const char* fmt, ...) { }

// Tests
///////////////////////////////////////////////////////////
static void prv_test_decode_encode(const char* test_name, char* buffer, unsigned int buffer_length,
                                   const uint8_t* expected, unsigned int expected_length) {
  cl_assert(buffer_length % 4 == 0);

  char original_in[buffer_length + 1];
  memcpy(original_in, buffer, buffer_length + 1);

  // test decode
  unsigned int num_bytes = base64_decode_inplace(buffer, buffer_length);
  cl_assert(num_bytes == expected_length);
  cl_assert(memcmp(buffer, expected, num_bytes) == 0);

  // test encode
  char out[buffer_length + 1];
  int result = base64_encode(out, buffer_length + 1, expected, expected_length);
  cl_assert_equal_i(result, buffer_length);
  cl_assert_equal_m(out, original_in, buffer_length);
}

void test_base64__initialize(void) {
}

void test_base64__cleanup(void) {
}

void test_base64__decode(void) {
  {
    char buffer[] = "abcd";
    const uint8_t expected[] = { 0x69, 0xb7, 0x1d };
    prv_test_decode_encode("basic", buffer, 4, expected, 3);
  }

  {
    char buffer[] = "ABCD";
    const uint8_t expected[] = { 0x0, 0x10, 0x83 };
    prv_test_decode_encode("upper", buffer, 4, expected, 3);
  }

  {
    char buffer[] = "abcdABCD";
    const uint8_t expected[] = { 0x69, 0xb7, 0x1d, 0x0, 0x10, 0x83 };
    prv_test_decode_encode("twobyte", buffer, 8, expected, 6);
  }

  {
    char buffer[] = "vu8=";
    const uint8_t expected[] = { 0xbe, 0xef };
    prv_test_decode_encode("1pad", buffer, 4, expected, 2);
  }

  {
    char buffer[] = "aQ==";
    const uint8_t expected[] = { 0x69 };
    prv_test_decode_encode("2pad", buffer, 4, expected, 1);
  }
}
