/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/resident_image.h>

ResidentImage *PBL_WEAK resident_image_create(GSize size, GBitmapFormat format,
                                              const GColor *palette, uint8_t palette_count) {
  return nullptr;
}

ResidentImage *PBL_WEAK resident_image_create_from_bitmap(const GBitmap *bitmap) {
  return nullptr;
}

ResidentImage *PBL_WEAK resident_image_create_from_cache(const GBitmap *cached) {
  return nullptr;
}

ResidentImage *PBL_WEAK resident_image_load_cached(ImageCacheClient client,
                                                   const ImageCacheKey *key, GSize size) {
  return nullptr;
}

void PBL_WEAK resident_image_destroy(ResidentImage *image) {
}

bool PBL_WEAK resident_image_write(ResidentImage *image, uint32_t offset, const uint8_t *data,
                                   size_t length) {
  return false;
}

bool PBL_WEAK resident_image_is_pinned(const ResidentImage *image) {
  return false;
}

GColor *PBL_WEAK resident_image_get_palette(ResidentImage *image) {
  return nullptr;
}

GSize PBL_WEAK resident_image_get_size(const ResidentImage *image) {
  return GSizeZero;
}

uint32_t PBL_WEAK resident_image_get_data_size(const ResidentImage *image) {
  return 0;
}

void PBL_WEAK resident_image_draw(GContext *ctx, const ResidentImage *image, const GRect *rect) {
}
