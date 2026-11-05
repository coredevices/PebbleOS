/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @internal
//! data type that's used to store row data infos in a space-efficient manner
typedef struct {
  uint32_t offset; // uint32_t needed for rectangular framebuffers > 65535 bytes
  uint16_t min_x;
  uint16_t max_x;
} GBitmapDataRowInfoInternal;

// The SDK exporter can't crawl the framebuffer headers.
#if !defined(SDK)

#include <board/display_platform.h>

// For backwards compatibility, new code should use PBL_DISPLAY_WIDTH and PBL_DISPLAY_HEIGHT
#if !defined(DISP_COLS) || !defined(DISP_ROWS)
#define DISP_COLS PBL_DISPLAY_WIDTH
#define DISP_ROWS PBL_DISPLAY_HEIGHT
#endif // DISP_COLS || DISP_ROWS

#if PBL_ROUND
//! Pixels hidden at each end of the rows of the top half, mirrored in the bottom half.
extern const uint8_t g_display_round_mask[];

//! Pixels hidden at each end of row @p y.
static inline uint16_t display_round_mask_inset(uint16_t y) {
  return g_display_round_mask[(y < DISP_ROWS / 2) ? y : (DISP_ROWS - 1 - y)];
}
#endif // PBL_ROUND

#endif // !SDK
