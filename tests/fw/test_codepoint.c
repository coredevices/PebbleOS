/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/fonts/codepoint.h"

#include "clar.h"

///////////////////////////////////////////////////////////
// Stubs

#include "stubs_logging.h"
#include "stubs_passert.h"

///////////////////////////////////////////////////////////
// Tests

void test_codepoint__initialize(void) {
}

void test_codepoint__cleanup(void) {
}

void test_codepoint__is_unicode_space(void) {
  // All Unicode space variants should be recognized
  cl_assert(codepoint_is_unicode_space(NO_BREAK_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(EN_QUAD_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(EM_QUAD_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(EN_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(EM_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(THREE_PER_EM_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(FOUR_PER_EM_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(SIX_PER_EM_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(FIGURE_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(PUNCTUATION_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(THIN_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(HAIR_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(NARROW_NO_BREAK_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(MEDIUM_MATHEMATICAL_SPACE_CODEPOINT));
  cl_assert(codepoint_is_unicode_space(IDEOGRAPHIC_SPACE_CODEPOINT));

  // Regular space and other characters should not match
  cl_assert(!codepoint_is_unicode_space(SPACE_CODEPOINT));
  cl_assert(!codepoint_is_unicode_space('A'));
  cl_assert(!codepoint_is_unicode_space(ZERO_WIDTH_SPACE_CODEPOINT));
  cl_assert(!codepoint_is_unicode_space(NEWLINE_CODEPOINT));
  cl_assert(!codepoint_is_unicode_space(ELLIPSIS_CODEPOINT));
}

void test_codepoint__end_of_word_breakable_spaces(void) {
  // Unicode spaces with TR14 class BA (Break After) should be word breaks
  cl_assert(codepoint_is_end_of_word(EN_QUAD_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(EM_QUAD_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(EN_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(EM_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(THREE_PER_EM_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(FOUR_PER_EM_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(SIX_PER_EM_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(PUNCTUATION_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(THIN_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(HAIR_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(MEDIUM_MATHEMATICAL_SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(IDEOGRAPHIC_SPACE_CODEPOINT));
}

void test_codepoint__end_of_word_non_breaking_spaces(void) {
  // Non-breaking spaces (TR14 class GL) must NOT be word breaks
  cl_assert(!codepoint_is_end_of_word(NO_BREAK_SPACE_CODEPOINT));
  cl_assert(!codepoint_is_end_of_word(FIGURE_SPACE_CODEPOINT));
  cl_assert(!codepoint_is_end_of_word(NARROW_NO_BREAK_SPACE_CODEPOINT));
}

void test_codepoint__end_of_word_existing(void) {
  // Existing word break characters should still work
  cl_assert(codepoint_is_end_of_word(NULL_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(NEWLINE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(SPACE_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(HYPHEN_CODEPOINT));
  cl_assert(codepoint_is_end_of_word(ZERO_WIDTH_SPACE_CODEPOINT));

  // Regular characters should not be word breaks
  cl_assert(!codepoint_is_end_of_word('A'));
  cl_assert(!codepoint_is_end_of_word('0'));
}
