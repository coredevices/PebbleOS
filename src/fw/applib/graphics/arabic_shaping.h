/* SPDX-FileCopyrightText: 2026 Ahmed Hussein */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/fonts/codepoint.h"
#include "utf8.h"

#include <stdbool.h>
#include <stddef.h>

//! Arabic letter contextual form types
typedef enum {
  ARABIC_FORM_ISOLATED = 0,  //!< Letter stands alone
  ARABIC_FORM_FINAL    = 1,  //!< End of word (connects right only)
  ARABIC_FORM_INITIAL  = 2,  //!< Beginning of word (connects left only)
  ARABIC_FORM_MEDIAL   = 3   //!< Middle of word (connects both sides)
} ArabicForm;

//! Check if a codepoint is a shapeable Arabic letter (U+0621-U+064A).
//! This excludes diacritics, numerals, and other non-letter characters.
//! @param cp The codepoint to check
//! @return true if the codepoint is a shapeable Arabic letter
bool arabic_is_shapeable(Codepoint cp);

//! Shape a single Arabic codepoint based on its neighbors.
//!
//! Returns the contextual presentation form for `curr_cp` given the
//! previous and next codepoints. Pass 0 for `prev_cp` / `next_cp` when
//! the letter is at a string or segment boundary. Non-Arabic codepoints
//! are returned unchanged, so this helper is safe to call for any
//! codepoint and acts as a no-op outside the shapeable range.
//!
//! Used by both the renderer and the layout/measurement path so that
//! width computation matches what is actually drawn.
Codepoint arabic_shape_codepoint(Codepoint prev_cp, Codepoint curr_cp, Codepoint next_cp);

//! Shape Arabic text by converting basic Arabic letters to their
//! contextual presentation forms based on position in words.
//!
//! This function MUST be called BEFORE RTL reversal. The shaping
//! process looks at neighboring letters to determine if each letter
//! should be isolated, initial, medial, or final form.
//!
//! @param src Source UTF-8 string containing Arabic text
//! @param src_len Length of source string in bytes
//! @param dest Destination buffer for shaped text
//! @param dest_size Size of destination buffer in bytes
//! @return Number of bytes written to dest (excluding null terminator),
//!         or 0 on failure
size_t arabic_shape_text(const utf8_t *src, size_t src_len,
                         utf8_t *dest, size_t dest_size);
