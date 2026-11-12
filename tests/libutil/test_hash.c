/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <string.h>

#include <pbl/util/hash.h>

void test_hash__empty(void) {
  cl_assert_equal_i(hash((const uint8_t *)"", 0), HASH_INIT);
}

void test_hash__known_value(void) {
  // 5381 * 33 + 'a'
  cl_assert_equal_i(hash((const uint8_t *)"a", 1), 177670);
}

void test_hash__chunks_match_whole(void) {
  const char *s = "the quick brown fox";
  const size_t len = strlen(s);
  uint32_t h = hash_update(HASH_INIT, s, 4);

  h = hash_update(h, s + 4, len - 4);
  cl_assert_equal_i(h, hash((const uint8_t *)s, len));
}
