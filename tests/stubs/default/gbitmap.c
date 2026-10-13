/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/graphics/gtypes.h>

PBL_WEAK uint8_t gbitmap_get_bits_per_pixel(GBitmapFormat format) {
  return 0;
}

PBL_WEAK void gbitmap_destroy(GBitmap *bitmap) {
}

PBL_WEAK GBitmapFormat gbitmap_get_format(const GBitmap *bitmap) {
  return bitmap->info.format;
}

PBL_WEAK GBitmap *gbitmap_create_with_resource_system(ResAppNum app_num, uint32_t resource_id) {
  return nullptr;
}

PBL_WEAK GBitmap *gbitmap_create_with_data(const uint8_t *data) {
  return nullptr;
}

PBL_WEAK GBitmap *gbitmap_create_blank(GSize size, GBitmapFormat format) {
  return nullptr;
}

PBL_WEAK GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap *bitmap, uint16_t y) {
  return (GBitmapDataRowInfo){};
}

PBL_WEAK uint16_t gbitmap_format_get_row_size_bytes(int16_t width, GBitmapFormat format) {
  return width;
}

PBL_WEAK GRect gbitmap_get_bounds(const GBitmap *bitmap) {
  return bitmap->bounds;
}

PBL_WEAK void gbitmap_init_as_sub_bitmap(GBitmap *sub_bitmap, const GBitmap *base_bitmap,
                                         GRect sub_rect) {
  // Mirrors the non-legacy (GBITMAP_VERSION_1) path of the production implementation: the sub
  // bitmap shares the base bitmap's data, owns none of it, and is clipped to the base bounds.
  // The compositor only ever passes the version-1 framebuffer bitmaps here.
  *sub_bitmap = *base_bitmap;
  sub_bitmap->info.is_palette_heap_allocated = false;
  sub_bitmap->info.is_bitmap_heap_allocated = false;
  grect_clip(&sub_rect, &base_bitmap->bounds);
  sub_bitmap->bounds = sub_rect;
}
