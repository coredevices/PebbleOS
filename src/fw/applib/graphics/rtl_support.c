/* SPDX-FileCopyrightText: 2026 Ahmed Hussein */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rtl_support.h"

#include "applib/fonts/codepoint.h"
#include "utf8.h"

#include <string.h>

// Maximum number of codepoints we can handle in a single reversal
// Keep small for stack safety on embedded systems (16 codepoints * 4 bytes = 64 bytes)
#define MAX_RTL_CODEPOINTS 16

bool utf8_contains_rtl(const utf8_t *start, const utf8_t *end) {
  if (start == NULL || end == NULL || start >= end) {
    return false;
  }

  utf8_t *ptr = (utf8_t *)start;
  while (ptr < end && *ptr != '\0') {
    utf8_t *next = NULL;
    Codepoint cp = utf8_peek_codepoint(ptr, &next);
    if (cp == 0 || next == NULL) {
      break;
    }
    if (codepoint_is_rtl(cp)) {
      return true;
    }
    ptr = next;
  }

  return false;
}

//! Check if a codepoint is a shapeable Arabic letter (U+0621-U+064A)
static bool prv_codepoint_is_arabic_letter(Codepoint cp) {
  // Arabic letters that require contextual shaping
  // Excludes diacritics (U+064B-U+065F) and numerals (U+0660-U+0669)
  return (cp >= 0x0621 && cp <= 0x064A);
}

bool utf8_contains_arabic(const utf8_t *start, const utf8_t *end) {
  if (start == NULL || end == NULL || start >= end) {
    return false;
  }

  utf8_t *ptr = (utf8_t *)start;
  while (ptr < end && *ptr != '\0') {
    utf8_t *next = NULL;
    Codepoint cp = utf8_peek_codepoint(ptr, &next);
    if (cp == 0 || next == NULL) {
      break;
    }
    if (prv_codepoint_is_arabic_letter(cp)) {
      return true;
    }
    ptr = next;
  }

  return false;
}

size_t utf8_reverse_for_rtl(const utf8_t *src, size_t src_len,
                            utf8_t *dest, size_t dest_size) {
  if (src == NULL || dest == NULL || src_len == 0 || dest_size == 0) {
    return 0;
  }

  // First pass: collect all codepoints into an array
  Codepoint codepoints[MAX_RTL_CODEPOINTS];
  size_t num_codepoints = 0;

  utf8_t *ptr = (utf8_t *)src;
  const utf8_t *end = src + src_len;

  while (ptr < end && *ptr != '\0' && num_codepoints < MAX_RTL_CODEPOINTS) {
    utf8_t *next = NULL;
    Codepoint cp = utf8_peek_codepoint(ptr, &next);
    if (cp == 0 || next == NULL) {
      break;
    }
    codepoints[num_codepoints++] = cp;
    ptr = next;
  }

  if (num_codepoints == 0) {
    return 0;
  }

  // Second pass: write codepoints in reverse order to destination
  size_t dest_offset = 0;

  for (size_t i = num_codepoints; i > 0; i--) {
    Codepoint cp = codepoints[i - 1];

    // Make sure we have room for at least 4 bytes + null terminator
    if (dest_offset + 4 >= dest_size) {
      break;
    }

    size_t bytes_written = utf8_encode_codepoint(cp, dest + dest_offset);
    if (bytes_written == 0) {
      continue; // Skip invalid codepoints
    }
    dest_offset += bytes_written;
  }

  // Null-terminate if we have space
  if (dest_offset < dest_size) {
    dest[dest_offset] = '\0';
  }

  return dest_offset;
}
