/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_jerry_port_common.h"
#include "test_rocky_common.h"
#include "applib/rockyjs/pbl_jerry_port.h"

#include "jerry-api.h"

#include <util/size.h>

#include <clar.h>
#include <stdio.h>

// Fakes
#include "fake_time.h"
#include "fake_logging.h"
#include "fake_pbl_malloc.h"

// Stubs
#include "stubs_app_state.h"
#include "stubs_logging.h"
#include "stubs_passert.h"

// Great read-up on JavaScript and its text encoding quirks:
// https://mathiasbynens.be/notes/javascript-unicode

void test_rocky_text_encoding__initialize(void) {
  fake_pbl_malloc_clear_tracking();
  rocky_runtime_context_init();
  jerry_init(JERRY_INIT_EMPTY);
}

void test_rocky_text_encoding__cleanup(void) {
  jerry_cleanup();
  rocky_runtime_context_deinit();
  fake_pbl_malloc_check_net_allocs();
}

void test_rocky_text_encoding__jerry_handles_cesu8_strings_in_source(void) {
  // Although CESU-8 and UTF-8 are not compatible on paper, JerryScript's lexer doesn't mind if
  // we feed it CESU-8 encoded strings... Test this, so we know when this changes in the future:
  EXECUTE_SCRIPT("var pileOfPooCESU8 = '\xed\xa0\xbd\xed\xb2\xa9';");
  // Expect a pair of surrogate code points:
  EXECUTE_SCRIPT_AND_ASSERT_RV_EQUALS_S("pileOfPooCESU8.charCodeAt(0).toString(16)", "d83d");
  EXECUTE_SCRIPT_AND_ASSERT_RV_EQUALS_S("pileOfPooCESU8.charCodeAt(1).toString(16)", "dca9");
}

void test_rocky_text_encoding__jerry_handles_utf8_strings_in_source(void) {
  // Source is be UTF-8 encoded.
  // Have a string variable with Pile of Poo (💩) or U+1F4A9 in it, encoded using 4-bytes:
  EXECUTE_SCRIPT("var pileOfPooUTF8 = '\xF0\x9F\x92\xA9';");
  // Expect a pair of surrogate code points:
  EXECUTE_SCRIPT_AND_ASSERT_RV_EQUALS_S("pileOfPooUTF8.charCodeAt(0).toString(16)", "d83d");
  EXECUTE_SCRIPT_AND_ASSERT_RV_EQUALS_S("pileOfPooUTF8.charCodeAt(1).toString(16)", "dca9");
}

void test_rocky_text_encoding__jerry_asserts_utf8_non_bmp_codepoint_in_identifier(void) {
  // It's forbidden to have an identifier contain a non-BMP codepoint (UTF-8 encoded):
  EXECUTE_SCRIPT_EXPECT_ERROR("var poo\xF0\x9F\x92\xA9poo = 'pileOfPoo';",
                              "SyntaxError: Invalid (unexpected) character. [line: 1, column: 8]");
}

void test_rocky_text_encoding__jerry_asserts_cesu8_non_bmp_codepoint_in_identifier(void) {
  // It's forbidden to have an identifier contain a non-BMP codepoint (CESU-8 encoded):
  EXECUTE_SCRIPT_EXPECT_ERROR("var poo\xed\xa0\xbd\xed\xb2\xa9poo = 'pileOfPoo';",
                              "SyntaxError: Invalid (unexpected) character. [line: 1, column: 8]");
}

void test_rocky_text_encoding__string_length(void) {
  EXECUTE_SCRIPT("var pileOfPooUTF8 = '\xF0\x9F\x92\xA9';");
  // String.length is expected to count the surrogate code points that make up a non-BMP codepoint:
  EXECUTE_SCRIPT_AND_ASSERT_RV_EQUALS_S("pileOfPooUTF8.length.toString()", "2");
}

void test_rocky_text_encoding__jerry_cesu8_to_utf8_conversion(void) {
  struct {
    const char *const script;
    size_t expected_utf_size;
    const char *const expected_utf_data;
  } cases[] = {
    [0] = {
      .script = "var str = '\\uDCA9';", // low surrogate only
      .expected_utf_size = 0,
    },
    [1] = {
      .script = "var str = '\\uD83D';", // high surrogate only
      .expected_utf_size = 0,
    },
    [2] = {
      .script = "var str = '\\uDCA9\\uD83D';", // reversed order
      .expected_utf_size = 0,
    },
    [3] = {
      .script = "var str = '\\uD83Dx\\uDCA9';", // non-surrogate in between pair
      .expected_utf_size = 1,
      .expected_utf_data = "x",
    },
    [4] = {
      .script = "var str = '\\uD83Dx';", // high surrogate followed by non-surrogate
      .expected_utf_size = 1,
      .expected_utf_data = "x",
    },
    [5] = {
      .script = "var str = '\\uDCA9x';", // low surrogate followed by non-surrogate
      .expected_utf_size = 1,
      .expected_utf_data = "x",
    },
    [6] = {
      .script = "var str = 'AB';",
      .expected_utf_size = 2,
      .expected_utf_data = "AB",
    },
    [7] = {
      .script = "var str = '\xC4\x91';", // 2-byte codepoint (U+0111)
      .expected_utf_size = 2,
      .expected_utf_data = "\xC4\x91",
    },
    [8] = {
      .script = "var str = '\xE0\xA0\x95';", // 3-byte codepoint (U+0815)
      .expected_utf_size = 3,
      .expected_utf_data = "\xE0\xA0\x95",
    },
    [9] = {
      .script = "var str = '\\uD83D\\uDCA9';", // 4-byte codepoint (U+1F4A9, escaped data)
      .expected_utf_size = 4,
      .expected_utf_data = "\xF0\x9F\x92\xA9",
    },
    [10] = {
      .script = "var str = '\xF0\x9F\x92\xA9';", // 4-byte codepoint (U+1F4A9, UTF-8 data in source)
      .expected_utf_size = 4,
      .expected_utf_data = "\xF0\x9F\x92\xA9",
    },
  };

  for (int j = 0; j < 2; ++j) {
    const bool is_overflow_test = (j == 1);
    for (int i = 0; i < ARRAY_LENGTH(cases); ++i) {
      printf("Case %i (is_overflow_test=%u): %s\n", i, is_overflow_test, cases[i].script);

      EXECUTE_SCRIPT(cases[i].script);
      const jerry_value_t s = JS_GLOBAL_GET_VALUE("str");

      const jerry_size_t utf8_size = jerry_get_utf8_string_size(s);
      // U+1F4A9 is expected to get encoded into 4 bytes of UTF-8:
      cl_assert_equal_i(utf8_size, cases[i].expected_utf_size);

      const size_t buffer_size = utf8_size ? (is_overflow_test ? (utf8_size - 1) : utf8_size) : 0;

      // malloc, so DUMA will detect buffer overflows:
      jerry_char_t *utf8_buffer = malloc(buffer_size);

      const jerry_size_t copied_size =
          jerry_string_to_utf8_char_buffer(s, utf8_buffer, buffer_size);
      if (!is_overflow_test) {
        cl_assert_equal_i(copied_size, cases[i].expected_utf_size);
        if (cases[i].expected_utf_size) {
          cl_assert_equal_m(utf8_buffer, cases[i].expected_utf_data, cases[i].expected_utf_size);
        }
      } else {
        // When buffer is too small, expect 0 bytes copied:
        cl_assert_equal_i(copied_size, 0);
      }
      jerry_release_value(s);

      free(utf8_buffer);
    }
  }
}

void test_rocky_text_encoding__jerry_utf8_to_cesu8_conversion(void) {
  struct {
    const char *const utf8_input;
    const char *const cesu8_output;
  } cases[] = {
    {
      .utf8_input = "",
      .cesu8_output = "",
    },
    {
      .utf8_input = "abc",
      .cesu8_output = "abc",
    },
    {
      // U+1F4A9 expands to surrogate pair:
      .utf8_input = "abc\xF0\x9F\x92\xA9xyz",
      .cesu8_output = "abc\xed\xa0\xbd\xed\xb2\xa9xyz",
    },
    {
      // Be lax with surrogates: even though they're not supposed to appear in UTF-8,
      // just copy them over to the CESU-8 output, even a "half pair":
      .utf8_input = "\xed\xa0\xbd",
      .cesu8_output = "\xed\xa0\xbd",
    },
  };
  for (int i = 0; i < ARRAY_LENGTH(cases); ++i) {
    jerry_char_t output[32] = {};
    const jerry_value_t s = jerry_create_string_utf8((const jerry_char_t *)cases[i].utf8_input);
    const jerry_size_t copied_bytes = jerry_string_to_char_buffer(s, output, sizeof(output));
    cl_assert_equal_i(copied_bytes, strlen(cases[i].cesu8_output));
    if (copied_bytes) {
      cl_assert_equal_m(output, cases[i].cesu8_output, copied_bytes);
    }
    // TODO: test equality/hash
    jerry_release_value(s);
  }
}
