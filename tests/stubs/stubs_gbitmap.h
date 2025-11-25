/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"

uint8_t gbitmap_get_bits_per_pixel(GBitmapFormat format){return 0;}

void gbitmap_destroy(GBitmap* bitmap){}

GBitmapFormat gbitmap_get_format(const GBitmap *bitmap) {
  return bitmap->info.format;
}

GBitmap *gbitmap_create_with_resource_system(ResAppNum app_num, uint32_t resource_id) {
  return NULL;
}

GBitmap *gbitmap_create_with_data(const uint8_t *data){ return NULL; }

GBitmap *gbitmap_create_blank(GSize size, GBitmapFormat format) { return NULL; }

GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap *bitmap, uint16_t y) {
  return (GBitmapDataRowInfo) {0};
}

uint16_t gbitmap_format_get_row_size_bytes(int16_t width, GBitmapFormat format) {
  return width;
}

GRect gbitmap_get_bounds(const GBitmap *bitmap) {
  return bitmap->bounds;
}
