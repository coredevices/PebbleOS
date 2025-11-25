/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_gbitmap_get_data_row.h"

#include "applib/graphics/gtypes.h"

#include <stdint.h>

bool s_fake_data_row_handling = false;
bool s_fake_data_row_handling_disable_vertical_flip = false;

extern GBitmapDataRowInfo prv_gbitmap_get_data_row_info(const GBitmap *bitmap, uint16_t y);

// Overrides the same function in gbitmap.c
GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap *bitmap, uint16_t y) {
  // If fake data row handling is enabled, clip the row to a diamond mask
  if (s_fake_data_row_handling) {
    const int16_t diamond_offset =
        ABS((bitmap->bounds.size.w / 2) - (y * bitmap->bounds.size.w / bitmap->bounds.size.h));
    const int16_t min_x = bitmap->bounds.origin.x + diamond_offset;
    // vertically flip unless disabled
    if (!s_fake_data_row_handling_disable_vertical_flip) {
      y = bitmap->bounds.size.h - y - 1;
    }
    return (GBitmapDataRowInfo){
      .data = prv_gbitmap_get_data_row_info(bitmap, y).data,
      .min_x = min_x,
      .max_x = grect_get_max_x(&bitmap->bounds) - diamond_offset - 1,
    };
  }
  return prv_gbitmap_get_data_row_info(bitmap, y);
}
