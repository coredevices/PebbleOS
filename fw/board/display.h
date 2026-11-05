/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// The SDK exporter can't crawl the framebuffer headers.
#if !defined(SDK)

#include <board/display_platform.h>

#if PBL_ROUND
//! Pixels hidden at each end of the rows of the top half, mirrored in the bottom half.
extern const uint8_t g_display_round_mask[];

//! Pixels hidden at each end of row @p y.
static inline uint16_t display_round_mask_inset(uint16_t y) {
  return g_display_round_mask[(y < PBL_DISPLAY_HEIGHT / 2) ? y : (PBL_DISPLAY_HEIGHT - 1 - y)];
}
#endif // PBL_ROUND

#endif // !SDK
