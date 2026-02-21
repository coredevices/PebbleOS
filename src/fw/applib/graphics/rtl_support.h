/* SPDX-FileCopyrightText: 2026 Ahmed Hussein */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "utf8.h"

#include <stdbool.h>
#include <stddef.h>

//! Check if a UTF-8 string range contains any RTL (right-to-left) characters.
//! This includes Arabic (U+0600-U+06FF) and Hebrew (U+0590-U+05FF) scripts.
//! @param start Pointer to start of UTF-8 string
//! @param end Pointer to end of UTF-8 string (exclusive)
//! @return true if the range contains at least one RTL character
bool utf8_contains_rtl(const utf8_t *start, const utf8_t *end);

//! Check if a UTF-8 string range contains any shapeable Arabic letters.
//! This checks for Arabic letters in range U+0621-U+064A which require
//! contextual shaping (excludes diacritics and numerals).
//! @param start Pointer to start of UTF-8 string
//! @param end Pointer to end of UTF-8 string (exclusive)
//! @return true if the range contains at least one shapeable Arabic letter
bool utf8_contains_arabic(const utf8_t *start, const utf8_t *end);

//! Reverse UTF-8 codepoints in a buffer for RTL display.
//! This performs a simple character-level reversal without complex text shaping.
//! @param src Source UTF-8 string
//! @param src_len Length of source string in bytes
//! @param dest Destination buffer for reversed string
//! @param dest_size Size of destination buffer in bytes
//! @return Number of bytes written to dest (excluding null terminator), or 0 on failure
size_t utf8_reverse_for_rtl(const utf8_t *src, size_t src_len,
                            utf8_t *dest, size_t dest_size);
