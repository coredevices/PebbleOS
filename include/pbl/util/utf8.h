/* SPDX-FileCopyrightText: 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de> */
/* SPDX-License-Identifier: MIT */

#pragma once

#include <stdint.h>

/**
 * @defgroup util_utf8 UTF-8
 * @ingroup util
 * @brief UTF-8 decoder (Bjoern Hoehrmann's DFA).
 * @{
 */

/** Decoder state: a code point has been decoded, or decoding has not started. */
#define PBL_UTF8_ACCEPT 0U
/** Decoder state: the input is not valid UTF-8. Sticky until the state is reset. */
#define PBL_UTF8_REJECT 12U

/**
 * @brief Feed one byte to the UTF-8 decoder.
 *
 * Start with @p state set to @ref PBL_UTF8_ACCEPT. A code point is complete when the returned
 * state is @ref PBL_UTF8_ACCEPT. Overlong forms, surrogates and code points above U+10FFFF are
 * rejected.
 *
 * @param[in,out] state Decoder state.
 * @param[in,out] codepoint Code point being decoded.
 * @param byte Next input byte.
 * @return New decoder state.
 */
uint8_t pbl_utf8_decode(uint8_t *state, uint32_t *codepoint, uint8_t byte);

/** @} */
