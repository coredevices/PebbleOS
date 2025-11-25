/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ecma-helpers.h"
#include "lit-strings.h"
#include "ecma-init-finalize.h"
#include "lit-char-helpers.h"
#include "js-parser-internal.h"

#include "test-common.h"

int
main ()
{
  TEST_INIT ();

  jmem_init ();
  ecma_init ();

  const uint8_t _1_byte_long1[] = "\\u007F";
  const uint8_t _1_byte_long2[] = "\\u0000";
  const uint8_t _1_byte_long3[] = "\\u0065";

  const uint8_t _2_byte_long1[] = "\\u008F";
  const uint8_t _2_byte_long2[] = "\\u00FF";
  const uint8_t _2_byte_long3[] = "\\u07FF";

  const uint8_t _3_byte_long1[] = "\\u08FF";
  const uint8_t _3_byte_long2[] = "\\u0FFF";
  const uint8_t _3_byte_long3[] = "\\uFFFF";

  size_t length;

  // test 1-byte-long unicode sequences
  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _1_byte_long1 + 2, 4));
  TEST_ASSERT (length == 1);

  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _1_byte_long2 + 2, 4));
  TEST_ASSERT (length == 1);

  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _1_byte_long3 + 2, 4));
  TEST_ASSERT (length == 1);

  // test 2-byte-long unicode sequences
  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _2_byte_long1 + 2, 4));
  TEST_ASSERT (length == 2);

  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _2_byte_long2 + 2, 4));
  TEST_ASSERT (length == 2);

  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _2_byte_long3 + 2, 4));
  TEST_ASSERT (length == 2);

  // test 3-byte-long unicode sequences
  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _3_byte_long1 + 2, 4));
  TEST_ASSERT (length != 2);

  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _3_byte_long2 + 2, 4));
  TEST_ASSERT (length == 3);

  length = lit_char_get_utf8_length (lexer_hex_to_character (0, _3_byte_long3 + 2, 4));
  TEST_ASSERT (length == 3);

  ecma_finalize ();
  jmem_finalize ();

  return 0;
} /* main */
