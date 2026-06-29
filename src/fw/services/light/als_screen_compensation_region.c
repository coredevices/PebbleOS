/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/light/als_screen_compensation.h"

#if defined(CONFIG_ALS_SCREEN_COMPENSATION)

#include "applib/graphics/gtypes.h"
#include "util/math.h"

#include <stddef.h>

// gcolor_get_luminance() returns a 2-bit value (0..3); scale to 0..255.
#define ALS_COMP_LUM_TO_255(l) ((uint32_t)(l) * 85u)

uint16_t als_compensation_region_luminance(const GBitmap *fb, int16_t rx, int16_t ry,
                                           int16_t rw, int16_t rh) {
  if (fb == NULL || rw <= 0 || rh <= 0) {
    return 256;  // empty/unmeasured region -> unity gain
  }

  const int16_t fb_w = fb->bounds.size.w;
  const int16_t fb_h = fb->bounds.size.h;
  const int16_t y0 = MAX(ry, (int16_t)0);
  const int16_t y1 = MIN((int16_t)(ry + rh), fb_h);

  uint32_t sum = 0;
  uint32_t count = 0;
  for (int16_t y = y0; y < y1; y++) {
    const GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, y);
    const int16_t x0 = MAX(MAX(rx, row.min_x), (int16_t)0);
    const int16_t x1 = MIN(MIN((int16_t)(rx + rw), (int16_t)(row.max_x + 1)), fb_w);
    for (int16_t x = x0; x < x1; x++) {
      const GColor8 c = (GColor8){.argb = row.data[x]};
      sum += ALS_COMP_LUM_TO_255(gcolor_get_luminance(c));
      count++;
    }
  }

  if (count == 0) {
    return 256;
  }
  const uint32_t avg255 = sum / count;          // 0..255
  return (uint16_t)((avg255 * 256u) / 255u);    // 0..256 (white -> 256)
}

#endif  // CONFIG_ALS_SCREEN_COMPENSATION
