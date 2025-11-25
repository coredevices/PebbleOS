/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/utf8.h"
#include "utf8_test_data.h"

#include "clar.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_passert.h"


// Tests
///////////////////////////////////////////////////////////

void test_utf8__decode_test_string_valid(void) {
  static const int NUM_VALID_CODEPOINTS = sizeof(s_valid_test_codepoints) / sizeof(uint32_t);

  bool is_valid = utf8_is_valid_string(s_valid_test_string);
  cl_assert(is_valid);

  utf8_t* valid_test_string_utf8 = (utf8_t*)s_valid_test_string;

  for (int i = 0; i < NUM_VALID_CODEPOINTS; ++i) {
    uint32_t decoded_codepoint = utf8_peek_codepoint(valid_test_string_utf8, NULL);
    uint32_t actual_codepoint = s_valid_test_codepoints[i];
    cl_assert_equal_i(decoded_codepoint, actual_codepoint);
    valid_test_string_utf8 = utf8_get_next(valid_test_string_utf8);
  }
}

void test_utf8__decode_malformed_test_string(void) {
  bool success = false;
  utf8_get_bounds(&success, s_malformed_test_string);
  cl_assert(!success);

  utf8_t* malformed_test_string_utf8 = (utf8_t*)s_malformed_test_string;

  for (int i = 0; i < (UTF8_TEST_MALFORMED_CODEPOINT_INDEX - 1); i++) {
    uint32_t decoded_codepoint = utf8_peek_codepoint(malformed_test_string_utf8, NULL);
    uint32_t actual_codepoint = s_valid_test_codepoints[i];
    cl_assert_equal_i(decoded_codepoint, actual_codepoint);
    malformed_test_string_utf8 = utf8_get_next(malformed_test_string_utf8);
  }

  // When we decode the invalid codepoint, it should return an invalid stream
  // error and set the pointer to the stream to be null
  cl_assert_equal_i(utf8_peek_codepoint(malformed_test_string_utf8, NULL), 0);
  cl_assert_(*malformed_test_string_utf8 == 0xcd, "Failed to invalidate an invalid UTF-8 test string");
}

void test_utf8__decode_all_gothic_codepoints(void) {
  static const int NUM_GOTHIC_CODEPOINTS = sizeof(s_valid_gothic_codepoints) / sizeof(uint32_t);

  bool is_valid = utf8_is_valid_string(s_valid_gothic_codepoints_string);
  cl_assert(is_valid);

  utf8_t* valid_gothic_codepoints_utf8 = (utf8_t*) s_valid_gothic_codepoints_string;

  for (int i = 0; i < NUM_GOTHIC_CODEPOINTS; i++) {
    uint32_t decoded_codepoint = utf8_peek_codepoint(valid_gothic_codepoints_utf8, NULL);
    uint32_t actual_codepoint = s_valid_gothic_codepoints[i];
    cl_assert_equal_i(decoded_codepoint, actual_codepoint);
    valid_gothic_codepoints_utf8 = utf8_get_next(valid_gothic_codepoints_utf8);
  }
}

void test_utf8__emoji_codepoints(void) {
  cl_assert(utf8_is_valid_string("\xF0\x9F\x98\x84"));
  cl_assert(utf8_is_valid_string("😃"));
}

void test_utf8__copy_single_byte_char(void) {
  utf8_t dest[5];
  memset(dest, 0, 5);

  size_t copied = utf8_copy_character(dest, (utf8_t *)"hello", 5);
  cl_assert_equal_i(copied, 1);
  cl_assert_equal_s((char *)dest, "h");
}

void test_utf8__copy_multibyte_char(void) {
  utf8_t dest[5];
  memset(dest, 0, 5);

  size_t copied = utf8_copy_character(dest, (utf8_t *)NIHAO, 5);

  cl_assert_equal_i(copied, NIHAO_FIRST_CHARACTER_BYTES);
  cl_assert_equal_s((char *)dest, NIHAO_FIRST_CHARACTER);
}

void test_utf8__copy_insufficient_space(void) {
  utf8_t dest[5];
  dest[0] = 0;

  size_t copied = utf8_copy_character(dest, (utf8_t *)NIHAO, 2);
  cl_assert_equal_i(copied, 0);
  cl_assert_equal_s((char *)dest, "");
}

void test_utf8__copy_fill_buffer(void) {
  utf8_t dest[5];
  memset(dest, 0, 5);

  size_t copied = utf8_copy_character(dest, (utf8_t *)NIHAO, 3);
  cl_assert_equal_i(copied, NIHAO_FIRST_CHARACTER_BYTES);
  cl_assert_equal_s((char *)dest, NIHAO_FIRST_CHARACTER);
}

void test_utf8__copy_last_character(void) {
  utf8_t dest[5];
  memset(dest, 0, 5);

  size_t copied = utf8_copy_character(dest, (utf8_t *)NIHAO_FIRST_CHARACTER, 5);

  cl_assert_equal_i(copied, NIHAO_FIRST_CHARACTER_BYTES);
  cl_assert_equal_s((char *)dest, NIHAO_FIRST_CHARACTER);
}

void test_utf8__copy_invalid_last_character(void) {
  utf8_t dest[5];
  memset(dest, 0, 5);

  size_t copied = utf8_copy_character(dest, (utf8_t *)"\xf0", 5);

  cl_assert_equal_i(copied, 0);
  cl_assert_equal_s((char *)dest, "");
}

void test_utf8__invalid_character(void) {
  utf8_t dest[5];
  memset(dest, 0, 5);

  size_t copied = utf8_copy_character(dest, (utf8_t *)"\xf0hi", 5);

  cl_assert_equal_i(copied, 0);
  cl_assert_equal_s((char *)dest, "");
}

void test_utf8__get_size_truncate(void) {
  cl_assert_equal_i(0, utf8_get_size_truncate("", 1));
  cl_assert_equal_i(0, utf8_get_size_truncate("", 100));
  cl_assert_equal_i(0, utf8_get_size_truncate(" ", 1));
  cl_assert_equal_i(2, utf8_get_size_truncate("ab", 3));
  cl_assert_equal_i(2, utf8_get_size_truncate("abc", 3));
  cl_assert_equal_i(17, utf8_get_size_truncate("Hello World! \xF0\x9F\x98\x84", 100));
  cl_assert_equal_i(13, utf8_get_size_truncate("Hello World! \xF0\x9F\x98\x84", 17));
  cl_assert_equal_i(16, utf8_get_size_truncate("Hello World! \xF0\x9F\x98", 17));
  cl_assert_equal_i(13, utf8_get_size_truncate("Hello World! \xF0\x9F\x98\x84", 16));
  cl_assert_passert(utf8_get_size_truncate("Hi", 0));
}

void test_utf8__truncate_with_ellipsis(void) {

  // basic smoke test
  char *output_buffer = malloc(6);
  size_t trunc_size = utf8_truncate_with_ellipsis("WWWWWWWWWWWWWWW", output_buffer, 6);
  cl_assert_equal_s(output_buffer, "WW\xe2\x80\xa6");
  cl_assert_equal_i(trunc_size, 6);

  // test where max_length < ellipsis_length
  output_buffer = realloc(output_buffer, 3);
  trunc_size = utf8_truncate_with_ellipsis("Hey", output_buffer, 3);
  cl_assert_equal_i(trunc_size, 0);

  // test where max_length == ellipsis_length
  output_buffer = realloc(output_buffer, 4);
  trunc_size = utf8_truncate_with_ellipsis("Hello", output_buffer, 4);
  cl_assert_equal_s(output_buffer, "\xe2\x80\xa6");
  cl_assert_equal_i(trunc_size, 4);

  // test where max_length == ellipsis_length + 1
  output_buffer = realloc(output_buffer, 5);
  trunc_size = utf8_truncate_with_ellipsis("Hello", output_buffer, 5);
  cl_assert_equal_s(output_buffer, "H\xe2\x80\xa6");
  cl_assert_equal_i(trunc_size, 5);

  // test that if we don't need to truncate, we don't
  output_buffer = realloc(output_buffer, 12);
  trunc_size = utf8_truncate_with_ellipsis("Hello there", output_buffer, 12);
  cl_assert_equal_s(output_buffer, "Hello there");
  cl_assert_equal_i(trunc_size, 12);

  // test that our utf8 support works properly and doesn't split multibyte characters
  output_buffer = realloc(output_buffer, 19);
  trunc_size = utf8_truncate_with_ellipsis("Hello World! \xF0\x9F\x98\x84 11111", output_buffer, 19);
  cl_assert_equal_s(output_buffer, "Hello World! \xe2\x80\xa6");
  cl_assert_equal_i(trunc_size, 17);

  // test that we access unallocated memory if the output buffer is too small
  output_buffer = realloc(output_buffer, 5);
  cl_assert_passert(utf8_truncate_with_ellipsis("Hello", output_buffer, 6));
  free(output_buffer);
}
