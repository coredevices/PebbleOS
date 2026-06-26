/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/rtl_support.h"
#include "applib/graphics/utf8.h"

#include "clar.h"

#include <string.h>

///////////////////////////////////////////////////////////
// Stubs

#include "stubs_logging.h"
#include "stubs_passert.h"

///////////////////////////////////////////////////////////
// Helpers

// Reverse `in` for RTL, decode the result into `cps`, return codepoint count.
static size_t prv_reverse(const char *in, Codepoint *cps, size_t max) {
  utf8_t out[128];
  size_t len = utf8_reverse_for_rtl((const utf8_t *)in, strlen(in), out, sizeof(out) - 1);
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

void test_rtl_support__initialize(void) {}
void test_rtl_support__cleanup(void) {}

///////////////////////////////////////////////////////////
// Tests

// Pure Arabic letters reverse to visual order: "ابج" -> ج ب ا.
void test_rtl_support__reverses_letters(void) {
  Codepoint cps[8];
  size_t n = prv_reverse("\xD8\xA7\xD8\xA8\xD8\xAC", cps, 8);  // Alef Beh Jeem
  cl_assert_equal_i(n, 3);
  cl_assert_equal_i(cps[0], 0x062C);  // Jeem
  cl_assert_equal_i(cps[1], 0x0628);  // Beh
  cl_assert_equal_i(cps[2], 0x0627);  // Alef
}

// Arabic-Indic digit runs keep left-to-right order (weak-LTR), not mirrored.
void test_rtl_support__arabic_indic_digits_preserved(void) {
  Codepoint cps[8];
  size_t n = prv_reverse("\xD9\xA2\xD9\xA0\xD9\xA2\xD9\xA6", cps, 8);  // ٢٠٢٦
  cl_assert_equal_i(n, 4);
  cl_assert_equal_i(cps[0], 0x0662);  // ٢
  cl_assert_equal_i(cps[1], 0x0660);  // ٠
  cl_assert_equal_i(cps[2], 0x0662);  // ٢
  cl_assert_equal_i(cps[3], 0x0666);  // ٦
}

// Western digits are weak-LTR too.
void test_rtl_support__western_digits_preserved(void) {
  Codepoint cps[8];
  size_t n = prv_reverse("123", cps, 8);
  cl_assert_equal_i(n, 3);
  cl_assert_equal_i(cps[0], '1');
  cl_assert_equal_i(cps[1], '2');
  cl_assert_equal_i(cps[2], '3');
}

// A digit run embedded in Arabic: letters reverse, the number stays in order.
// "ا٢٣" -> the digit run ٢٣ is emitted first (it ends up left of the letter),
// in logical order, then the Alef.
void test_rtl_support__digits_in_arabic(void) {
  Codepoint cps[8];
  size_t n = prv_reverse("\xD8\xA7\xD9\xA2\xD9\xA3", cps, 8);  // Alef ٢ ٣
  cl_assert_equal_i(n, 3);
  cl_assert_equal_i(cps[0], 0x0662);  // ٢
  cl_assert_equal_i(cps[1], 0x0663);  // ٣
  cl_assert_equal_i(cps[2], 0x0627);  // Alef
}

// A date keeps its slash-separated groups in order: the separators travel with
// the numeric run rather than reversing the groups (٢٠٢٦/٠٦/٢٢, not ٢٢/٠٦/٢٠٢٦).
void test_rtl_support__date_separators_preserved(void) {
  Codepoint cps[16];
  size_t n = prv_reverse("\xD9\xA2\xD9\xA0\xD9\xA2\xD9\xA6/\xD9\xA0\xD9\xA6/"
                         "\xD9\xA2\xD9\xA2", cps, 16);  // ٢٠٢٦/٠٦/٢٢
  cl_assert_equal_i(n, 10);
  const Codepoint expect[] = {0x0662, 0x0660, 0x0662, 0x0666, '/',
                              0x0660, 0x0666, '/', 0x0662, 0x0662};
  for (size_t i = 0; i < n; i++) {
    cl_assert_equal_i(cps[i], expect[i]);
  }
}

// A time keeps its colon-separated groups in order (١٢:٣٤, not ٣٤:١٢).
void test_rtl_support__time_separators_preserved(void) {
  Codepoint cps[8];
  size_t n = prv_reverse("\xD9\xA1\xD9\xA2:\xD9\xA3\xD9\xA4", cps, 8);  // ١٢:٣٤
  cl_assert_equal_i(n, 5);
  const Codepoint expect[] = {0x0661, 0x0662, ':', 0x0663, 0x0664};
  for (size_t i = 0; i < n; i++) {
    cl_assert_equal_i(cps[i], expect[i]);
  }
}

// A separator only joins the run between two digits. Here "/" has a letter on
// one side, so it is not part of the number and reverses normally: ا/٢ -> ٢ / ا.
void test_rtl_support__separator_needs_two_digits(void) {
  Codepoint cps[8];
  size_t n = prv_reverse("\xD8\xA7/\xD9\xA2", cps, 8);  // Alef / ٢
  cl_assert_equal_i(n, 3);
  cl_assert_equal_i(cps[0], 0x0662);  // ٢
  cl_assert_equal_i(cps[1], '/');
  cl_assert_equal_i(cps[2], 0x0627);  // Alef
}

// rtl_segment_content_end: byte offset of the trailing-space boundary.
static size_t prv_content_len(const char *str) {
  utf8_t *start = (utf8_t *)str;
  utf8_t *end = start + strlen(str);
  return (size_t)(rtl_segment_content_end(start, end) - start);
}

void test_rtl_support__content_end_no_trailing_space(void) {
  cl_assert_equal_i(prv_content_len("abc"), 3);
}

void test_rtl_support__content_end_single_trailing_space(void) {
  cl_assert_equal_i(prv_content_len("abc "), 3);
}

// Trailing spaces peel; interior spaces stay with content.
void test_rtl_support__content_end_interior_vs_trailing(void) {
  cl_assert_equal_i(prv_content_len("ab cd   "), 5);  // boundary after 'd'
}

// All-space run has no content -> boundary is the start (segment kept whole).
void test_rtl_support__content_end_all_spaces(void) {
  cl_assert_equal_i(prv_content_len("   "), 0);
  cl_assert_equal_i(prv_content_len(""), 0);
}

// Boundary lands on a codepoint start, not mid-sequence. "بر " = Beh Reh SP.
void test_rtl_support__content_end_multibyte(void) {
  cl_assert_equal_i(prv_content_len("\xD8\xA8\xD8\xB1 "), 4);  // two 2-byte cps, then space
}
