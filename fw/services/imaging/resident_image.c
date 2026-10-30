/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/psram.h>
#include <pbl/logging/logging.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/services/image_cache.h>
#include <pbl/services/resident_image.h>
#include <pbl/util/math.h>

#include <applib/graphics/graphics_bitmap.h>
#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DECLARE(service_imaging, DEFAULT_LOG_LEVEL);

struct ResidentImage {
  //! Image cache entry drawn in place, with the PSRAM held, when there was no RAM for a copy
  const GBitmap *pinned;
  GSize size;
  GBitmapFormat format;
  uint16_t row_size_bytes;
  uint16_t rows_per_segment;
  uint16_t num_segments;
  GColor palette[RESIDENT_IMAGE_MAX_PALETTE];
  uint8_t *segments[];
};

static uint32_t prv_segment_size(const ResidentImage *image) {
  return (uint32_t)image->row_size_bytes * image->rows_per_segment;
}

static uint16_t prv_segment_rows(const ResidentImage *image, uint16_t segment) {
  const int rows_before = segment * image->rows_per_segment;
  return MIN(image->rows_per_segment, image->size.h - rows_before);
}

ResidentImage *resident_image_create(GSize size, GBitmapFormat format, const GColor *palette,
                                     uint8_t palette_count) {
  if ((size.w <= 0) || (size.h <= 0) || (format == GBitmapFormat8BitCircular) ||
      (palette_count > RESIDENT_IMAGE_MAX_PALETTE)) {
    return NULL;
  }

  const uint16_t row_size_bytes = gbitmap_format_get_row_size_bytes(size.w, format);
  if (row_size_bytes == 0U) {
    return NULL;
  }

  const uint16_t rows_per_segment = MAX(1U, RESIDENT_IMAGE_SEGMENT_SIZE / row_size_bytes);
  const uint16_t num_segments = (size.h + rows_per_segment - 1U) / rows_per_segment;

  ResidentImage *image = kernel_zalloc(sizeof(*image) + num_segments * sizeof(image->segments[0]));
  if (!image) {
    return NULL;
  }

  *image = (ResidentImage){
    .size = size,
    .format = format,
    .row_size_bytes = row_size_bytes,
    .rows_per_segment = rows_per_segment,
    .num_segments = num_segments,
  };
  if (palette_count > 0U) {
    memcpy(image->palette, palette, palette_count * sizeof(GColor));
  }

  for (uint16_t i = 0; i < num_segments; i++) {
    image->segments[i] = kernel_malloc((size_t)row_size_bytes * prv_segment_rows(image, i));
    if (!image->segments[i]) {
      resident_image_destroy(image);
      return NULL;
    }
  }

  return image;
}

ResidentImage *resident_image_create_from_bitmap(const GBitmap *bitmap) {
  const GBitmapFormat format = bitmap->info.format;
  const GSize size = bitmap->bounds.size;
  uint8_t palette_count = 0;

  switch (format) {
    case GBitmapFormat1BitPalette:
      palette_count = 2;
      break;
    case GBitmapFormat2BitPalette:
      palette_count = 4;
      break;
    case GBitmapFormat4BitPalette:
      palette_count = 16;
      break;
    default:
      break;
  }

  if (bitmap->bounds.origin.x != 0) {
    return NULL;
  }

  ResidentImage *image = resident_image_create(size, format, bitmap->palette, palette_count);
  if (!image) {
    return NULL;
  }

  const uint8_t *row =
      (const uint8_t *)bitmap->addr + bitmap->bounds.origin.y * bitmap->row_size_bytes;
  PBL_ASSERTN(bitmap->row_size_bytes >= image->row_size_bytes);
  for (int16_t y = 0; y < size.h; y++) {
    resident_image_write(image, (uint32_t)y * image->row_size_bytes, row, image->row_size_bytes);
    row += bitmap->row_size_bytes;
  }

  return image;
}

ResidentImage *resident_image_create_from_cache(const GBitmap *cached) {
  ResidentImage *image = resident_image_create_from_bitmap(cached);
  if (image) {
    return image;
  }

  // Keep showing it from the PSRAM instead, at the cost of keeping it retained
  image = kernel_zalloc(sizeof(*image));
  if (!image) {
    return NULL;
  }
  PBL_LOG_WRN("No RAM for a %dx%d image, keeping it in PSRAM", cached->bounds.size.w,
              cached->bounds.size.h);
  PBL_ANALYTICS_ADD(psram_demote_fail, 1);
  psram_retain_acquire();
  image_cache_retain(cached);
  image->pinned = cached;
  image->size = cached->bounds.size;
  image->format = cached->info.format;
  return image;
}

ResidentImage *resident_image_load_cached(ImageCacheClient client, const ImageCacheKey *key,
                                          GSize size) {
  ResidentImage *image = NULL;

  // Spare the PSRAM a wake-up on a miss
  if (!image_cache_contains(client, key, size)) {
    return NULL;
  }

  psram_retain_acquire();
  const GBitmap *cached = image_cache_acquire(client, key, size);
  if (cached) {
    image = resident_image_create_from_cache(cached);
    image_cache_release(cached);
  }
  psram_retain_release();

  return image;
}

void resident_image_destroy(ResidentImage *image) {
  if (!image) {
    return;
  }

  if (image->pinned) {
    image_cache_release(image->pinned);
    psram_retain_release();
  }

  for (uint16_t i = 0; i < image->num_segments; i++) {
    kernel_free(image->segments[i]);
  }
  kernel_free(image);
}

bool resident_image_write(ResidentImage *image, uint32_t offset, const uint8_t *data,
                          size_t length) {
  if (image->pinned) {
    return false;
  }

  const uint32_t segment_size = prv_segment_size(image);
  if ((offset > resident_image_get_data_size(image)) ||
      (length > resident_image_get_data_size(image) - offset)) {
    return false;
  }

  while (length > 0U) {
    const uint16_t segment = offset / segment_size;
    const uint32_t within = offset % segment_size;
    const size_t n = MIN(length, segment_size - within);
    memcpy(image->segments[segment] + within, data, n);
    offset += n;
    data += n;
    length -= n;
  }

  return true;
}

bool resident_image_is_pinned(const ResidentImage *image) {
  return image->pinned != NULL;
}

GColor *resident_image_get_palette(ResidentImage *image) {
  return image->pinned ? image->pinned->palette : image->palette;
}

GSize resident_image_get_size(const ResidentImage *image) {
  return image->size;
}

uint32_t resident_image_get_data_size(const ResidentImage *image) {
  return (uint32_t)image->row_size_bytes * image->size.h;
}

void resident_image_draw(GContext *ctx, const ResidentImage *image, const GRect *rect) {
  if (image->pinned) {
    GBitmap bitmap = *image->pinned;
    bitmap.bounds.size = GSize(MIN(rect->size.w, image->size.w), MIN(rect->size.h, image->size.h));
    graphics_draw_bitmap_in_rect(ctx, &bitmap, &(GRect){rect->origin, bitmap.bounds.size});
    return;
  }

  GBitmap bitmap = {
    .row_size_bytes = image->row_size_bytes,
    .info.format = image->format,
    .info.version = GBITMAP_VERSION_CURRENT,
    .palette = (GColor *)image->palette,
  };
  const int16_t width = MIN(rect->size.w, image->size.w);
  const int16_t height = MIN(rect->size.h, image->size.h);

  for (uint16_t i = 0; i < image->num_segments; i++) {
    const int16_t y = i * image->rows_per_segment;
    if (y >= height) {
      break;
    }
    const int16_t rows = MIN(prv_segment_rows(image, i), height - y);
    bitmap.addr = image->segments[i];
    bitmap.bounds = GRect(0, 0, width, rows);
    graphics_draw_bitmap_in_rect(ctx, &bitmap,
                                 &GRect(rect->origin.x, rect->origin.y + y, width, rows));
  }
}
