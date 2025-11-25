/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t Codepoint;

bool codepoint_is_formatting_indicator(const Codepoint codepoint) {
  return false;
}

bool codepoint_is_skin_tone_modifier(const Codepoint codepoint) {
  return false;
}

bool codepoint_is_end_of_word(const Codepoint codepoint) {
  return false;
}

bool codepoint_is_ideograph(const Codepoint codepoint) {
  return false;
}

bool codepoint_should_skip(const Codepoint codepoint) {
  return false;
}

bool codepoint_is_zero_width(const Codepoint codepoint) {
  return false;
}

bool codepoint_is_latin(const Codepoint codepoint) {
  return false;
}

bool codepoint_is_emoji(const Codepoint codepoint) {
  return false;
}

bool codepoint_is_special(const Codepoint codepoint) {
  return false;
}
