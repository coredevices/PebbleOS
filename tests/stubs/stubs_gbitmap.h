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

void gbitmap_init_as_sub_bitmap(GBitmap *sub_bitmap, const GBitmap *base_bitmap, GRect sub_rect) {
  // Mirrors the non-legacy (GBITMAP_VERSION_1) path of the production implementation: the sub
  // bitmap shares the base bitmap's data, owns none of it, and is clipped to the base bounds.
  // The compositor only ever passes the version-1 framebuffer bitmaps here.
  *sub_bitmap = *base_bitmap;
  sub_bitmap->info.is_palette_heap_allocated = false;
  sub_bitmap->info.is_bitmap_heap_allocated = false;
  grect_clip(&sub_rect, &base_bitmap->bounds);
  sub_bitmap->bounds = sub_rect;
}
