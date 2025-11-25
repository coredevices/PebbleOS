/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "codepoint.h"

#include "util/size.h"

#include <stddef.h>

#define MAX_LATIN_CODEPOINT 0x02AF
#define MIN_SOFTBANK_EMOJI_CODEPOINT 0xE000
#define MAX_SOFTBANK_EMOJI_CODEPOINT 0xE537
#define MIN_UNIFIED_EMOJI_CODEPOINT 0x1F300
#define MAX_UNIFIED_EMOJI_CODEPOINT 0x1F6FF
#define MIN_SYMBOLS_CODEPOINT 0x2000
#define MAX_SYMBOLS_CODEPOINT 0x2BFF
#define MIN_IDEOGRAPH_CODEPOINT 0x2e80
#define MIN_SPECIAL_CODEPOINT 0xE0A0
#define MAX_SPECIAL_CODEPOINT 0xE0A2
#define MIN_SKIN_TONE_CODEPOINT 0x1F3FB
#define MAX_SKIN_TONE_CODEPOINT 0x1F3FF

// Note: Please keep these sorted
static const Codepoint NONSTANDARD_EMOJI_CODEPOINTS[] = {
    0x2192, // rightwards_arrow
    0x25BA, // black_right_pointing_pointer
    0x2605, // black_star
    0x260E, // black_telephone
    0x261D, // white_up_pointing_index
    0x263A, // white_smiling_face
    0x270A, // raised_fist
    0x270B, // raised_hand
    0x270C, // victory_hand
    0x2764, // heavy_black_heart
};

// Note: Please keep these sorted
static const Codepoint END_OF_WORD_CODEPOINTS[] = {
  NULL_CODEPOINT, // 0x0
  NEWLINE_CODEPOINT, // 0xa
  SPACE_CODEPOINT, // 0x20
  HYPHEN_CODEPOINT, // 0x2d
  ZERO_WIDTH_SPACE_CODEPOINT // 0x200b
};

//  Note: Please keep these sorted
static const Codepoint FORMATTING_CODEPOINTS[] = {
  0x7F,   // delete
  0x200C, // zero-width non-joiner
  0x200D, // zero-width joiner
  0x200E, // left to right
  0x200F, // right to left
  0x202A, // bidirectional - right to left
  0x202C, // bidirectional - pop direction
  0x202D, // left to right override
  0xFE0E, // variation selector 1
  0xFE0F, // variation selector 2
  0xFEFF, // zero-width-no-break
};

// Note: Please keep these sorted
static const Codepoint ZERO_WIDTH_CODEPOINTS[] = {
  ZERO_WIDTH_SPACE_CODEPOINT,
  WORD_JOINER_CODEPOINT,
};

static bool codepoint_in_list(const Codepoint codepoint, const Codepoint *codepoints, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (codepoints[i] >= codepoint) {
      return (codepoints[i] == codepoint);
    }
  }

  return false;
}

bool codepoint_is_formatting_indicator(const Codepoint codepoint) {
  return codepoint_in_list(codepoint, FORMATTING_CODEPOINTS, ARRAY_LENGTH(FORMATTING_CODEPOINTS));
}

bool codepoint_is_ideograph(const Codepoint codepoint) {
  if (codepoint > MIN_IDEOGRAPH_CODEPOINT) {
    // non ideographic characters. This is an approximation that is good enough until
    // we start supporting some exotic scripts (e.g. tibetan)
    return true;
  } else {
    return false;
  }
}

// see http://www.unicode.org/reports/tr14/ for the whole enchilada
bool codepoint_is_end_of_word(const Codepoint codepoint) {
  return codepoint_in_list(codepoint, END_OF_WORD_CODEPOINTS, ARRAY_LENGTH(END_OF_WORD_CODEPOINTS));
}

// see http://unicode.org/reports/tr51/ section 2.2 "Diversity"
bool codepoint_is_skin_tone_modifier(const Codepoint codepoint) {
  return (codepoint >= MIN_SKIN_TONE_CODEPOINT && codepoint <= MAX_SKIN_TONE_CODEPOINT);
}

bool codepoint_should_skip(const Codepoint codepoint) {
  return ((codepoint < 0x20 && codepoint != NEWLINE_CODEPOINT) ||
          (codepoint_is_skin_tone_modifier(codepoint)));
}

bool codepoint_is_zero_width(const Codepoint codepoint) {
  return codepoint_in_list(codepoint, ZERO_WIDTH_CODEPOINTS, ARRAY_LENGTH(ZERO_WIDTH_CODEPOINTS));
}

bool codepoint_is_latin(const Codepoint codepoint) {
  return (codepoint <= MAX_LATIN_CODEPOINT ||
         (codepoint >= MIN_SYMBOLS_CODEPOINT &&
          codepoint <= MAX_SYMBOLS_CODEPOINT));
}

bool codepoint_is_emoji(const Codepoint codepoint) {
  // search for the codepoint in the list of nonstandard emoji codepoints first.
  const bool found = codepoint_in_list(codepoint,
                                       NONSTANDARD_EMOJI_CODEPOINTS,
                                       ARRAY_LENGTH(NONSTANDARD_EMOJI_CODEPOINTS));
  if (found) {
    return true;
  } else {
    return ((codepoint >= MIN_SOFTBANK_EMOJI_CODEPOINT &&
             codepoint <= MAX_SOFTBANK_EMOJI_CODEPOINT) ||
            (codepoint >= MIN_UNIFIED_EMOJI_CODEPOINT &&
             codepoint <= MAX_UNIFIED_EMOJI_CODEPOINT));
  }
}

bool codepoint_is_special(const Codepoint codepoint) {
  return (codepoint >= MIN_SPECIAL_CODEPOINT &&
          codepoint <= MAX_SPECIAL_CODEPOINT);
}
