/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/arabic_shaping.h"
#include "applib/graphics/utf8.h"

#include "clar.h"

#include <string.h>

///////////////////////////////////////////////////////////
// Stubs

#include "stubs_logging.h"
#include "stubs_passert.h"

///////////////////////////////////////////////////////////
// Helpers

// Shape `in`, decode the result into `cps`, return the codepoint count.
static size_t prv_shape(const char *in, Codepoint *cps, size_t max) {
  utf8_t out[128];
  size_t len = arabic_shape_text((const utf8_t *)in, strlen(in), out, sizeof(out) - 1);
  out[len] = '\0';
  size_t count = 0;
  utf8_t *ptr = out;
  while (*ptr != '\0' && count < max) {
    utf8_t *next = NULL;
    Codepoint cp = utf8_peek_codepoint(ptr, &next);
    if (cp == 0 || next == NULL) {
      break;
    }
    cps[count++] = cp;
    ptr = next;
  }
  return count;
}

void test_arabic_shaping__initialize(void) {}
void test_arabic_shaping__cleanup(void) {}

///////////////////////////////////////////////////////////
// Tests

// "لا" = Lam (U+0644) + Alef (U+0627) -> isolated Lam-Alef ligature U+FEFB.
void test_arabic_shaping__lam_alef_isolated(void) {
  Codepoint cps[8];
  size_t n = prv_shape("\xD9\x84\xD8\xA7", cps, 8);
  cl_assert_equal_i(n, 1);
  cl_assert_equal_i(cps[0], 0xFEFB);
}

// "بلا" = Beh + Lam + Alef. Beh joins forward, so the ligature takes its
// final form U+FEFC, preceded by Beh in initial form U+FE91.
void test_arabic_shaping__lam_alef_final_after_connector(void) {
  Codepoint cps[8];
  size_t n = prv_shape("\xD8\xA8\xD9\x84\xD8\xA7", cps, 8);
  cl_assert_equal_i(n, 2);
  cl_assert_equal_i(cps[0], 0xFE91);  // Beh initial
  cl_assert_equal_i(cps[1], 0xFEFC);  // Lam-Alef final ligature
}

// "سلام" = Seen Lam Alef Meem -> Seen initial, Lam-Alef final ligature, Meem
// isolated (Alef does not join forward). Four input letters, three glyphs.
void test_arabic_shaping__salaam_has_ligature(void) {
  Codepoint cps[8];
  size_t n = prv_shape("\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85", cps, 8);
  cl_assert_equal_i(n, 3);
  cl_assert_equal_i(cps[0], 0xFEB3);  // Seen initial
  cl_assert_equal_i(cps[1], 0xFEFC);  // Lam-Alef final
  cl_assert_equal_i(cps[2], 0xFEE1);  // Meem isolated
}

// "مكمّلات" = Meem Kaf Meem shadda Lam Alef Teh. The shadda is transparent for
// joining, so the Meem stays medial and the Lam-Alef keeps its final form
// U+FEFC, not isolated U+FEFB.
void test_arabic_shaping__diacritic_transparent_join(void) {
  Codepoint cps[12];
  size_t n = prv_shape("\xD9\x85\xD9\x83\xD9\x85\xD9\x91\xD9\x84\xD8\xA7\xD8\xAA", cps, 12);
  cl_assert_equal_i(n, 6);            // seven letters, Lam-Alef ligates into one
  cl_assert_equal_i(cps[2], 0xFEE4);  // Meem medial (joins forward through the shadda)
  cl_assert_equal_i(cps[3], 0x0651);  // shadda preserved, in place
  cl_assert_equal_i(cps[4], 0xFEFC);  // Lam-Alef final (connected), not isolated FEFB
}

// Alef with Hamza above (U+0623) maps to its own ligature pair (U+FEF7/FEF8).
void test_arabic_shaping__lam_alef_hamza_variant(void) {
  Codepoint cps[8];
  size_t n = prv_shape("\xD9\x84\xD8\xA3", cps, 8);  // Lam + Alef-Hamza-above
  cl_assert_equal_i(n, 1);
  cl_assert_equal_i(cps[0], 0xFEF7);
}

// Non-Arabic text passes through untouched.
void test_arabic_shaping__ascii_passthrough(void) {
  Codepoint cps[8];
  size_t n = prv_shape("Hi", cps, 8);
  cl_assert_equal_i(n, 2);
  cl_assert_equal_i(cps[0], 'H');
  cl_assert_equal_i(cps[1], 'i');
}

// arabic_shape_pair: Lam + Alef returns the ligature and flags the Alef
// consumed; anything else behaves like arabic_shape_codepoint with no consume.
void test_arabic_shaping__shape_pair_consumes_alef(void) {
  bool consumed = false;
  Codepoint cp = arabic_shape_pair(0, 0x0644 /* Lam */, 0x0627 /* Alef */, &consumed);
  cl_assert_equal_i(cp, 0xFEFB);  // isolated Lam-Alef
  cl_assert(consumed);

  consumed = true;  // ensure it gets cleared
  cp = arabic_shape_pair(0, 0x0628 /* Beh */, 0x0644 /* Lam */, &consumed);
  cl_assert(!consumed);
  cl_assert_equal_i(cp, arabic_shape_codepoint(0, 0x0628, 0x0644));
}
