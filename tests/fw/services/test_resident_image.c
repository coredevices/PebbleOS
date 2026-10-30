/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/services/resident_image.h>

#include <applib/graphics/graphics_bitmap.h>
#include <clar.h>
#include <stubs_logging.h>

static int s_live_allocations;
static int s_fail_after = -1;

void *kernel_malloc(size_t bytes) {
  if (s_fail_after == 0) {
    return NULL;
  }
  if (s_fail_after > 0) {
    s_fail_after--;
  }
  s_live_allocations++;
  return malloc(bytes);
}

void *kernel_zalloc(size_t bytes) {
  void *ptr = kernel_malloc(bytes);
  if (ptr) {
    memset(ptr, 0, bytes);
  }
  return ptr;
}

void kernel_free(void *ptr) {
  if (ptr) {
    s_live_allocations--;
  }
  free(ptr);
}

uint16_t gbitmap_format_get_row_size_bytes(int16_t width, GBitmapFormat format) {
  switch (format) {
    case GBitmapFormat4BitPalette:
      return (width + 1) / 2;
    case GBitmapFormat8Bit:
      return width;
    default:
      return 0;
  }
}

// Reassembles what is drawn, one byte per pixel row byte
#define CANVAS_W 300
#define CANVAS_H 300
static uint8_t s_canvas[CANVAS_H][CANVAS_W];
static int s_draws;

void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, const GRect *rect) {
  s_draws++;
  cl_assert_equal_i(bitmap->bounds.size.w, rect->size.w);
  cl_assert_equal_i(bitmap->bounds.size.h, rect->size.h);
  for (int y = 0; y < rect->size.h; y++) {
    memcpy(&s_canvas[rect->origin.y + y][rect->origin.x],
           (const uint8_t *)bitmap->addr + y * bitmap->row_size_bytes, bitmap->row_size_bytes);
  }
}

void test_resident_image__initialize(void) {
  s_live_allocations = 0;
  s_fail_after = -1;
  s_draws = 0;
  memset(s_canvas, 0, sizeof(s_canvas));
}

void test_resident_image__cleanup(void) {
  cl_assert_equal_i(s_live_allocations, 0);
}

void test_resident_image__segments_hold_whole_rows(void) {
  // 260x260 4-bpp: 130 B rows, 3 rows per 512 B segment
  ResidentImage *image = resident_image_create(GSize(260, 260), GBitmapFormat4BitPalette, NULL, 0);
  cl_assert(image);
  cl_assert_equal_i(resident_image_get_data_size(image), 130 * 260);
  // Header plus 87 segments
  cl_assert_equal_i(s_live_allocations, 1 + 87);
  resident_image_destroy(image);
}

void test_resident_image__write_and_draw_round_trip(void) {
  const GSize size = GSize(200, 37);
  const uint32_t length = 200 * 37;
  uint8_t *pixels = malloc(length);
  for (uint32_t i = 0; i < length; i++) {
    pixels[i] = (uint8_t)(i * 7 + 3);
  }

  ResidentImage *image = resident_image_create(size, GBitmapFormat8Bit, NULL, 0);
  cl_assert(image);
  // Chunks that straddle segments
  for (uint32_t offset = 0; offset < length; offset += 333) {
    const uint32_t n = (length - offset < 333) ? length - offset : 333;
    cl_assert(resident_image_write(image, offset, pixels + offset, n));
  }
  cl_assert(!resident_image_write(image, length - 1, pixels, 2));

  resident_image_draw(NULL, image, &GRect(10, 20, 200, 37));
  cl_assert_equal_i(s_draws, (37 + 1) / 2);
  for (int y = 0; y < 37; y++) {
    cl_assert_equal_m(&s_canvas[20 + y][10], pixels + y * 200, 200);
  }

  resident_image_destroy(image);
  free(pixels);
}

void test_resident_image__draw_clips_to_rect(void) {
  ResidentImage *image = resident_image_create(GSize(100, 100), GBitmapFormat8Bit, NULL, 0);
  resident_image_draw(NULL, image, &GRect(0, 0, 50, 11));
  // 5 rows per segment: 5 + 5 + 1
  cl_assert_equal_i(s_draws, 3);
  resident_image_destroy(image);
}

void test_resident_image__copy_from_bitmap(void) {
  uint8_t pixels[4 * 10];
  for (unsigned int i = 0; i < sizeof(pixels); i++) {
    pixels[i] = i;
  }
  GColor palette[16] = {{.argb = 0xc0}, {.argb = 0xff}};
  GBitmap bitmap = {
    .addr = pixels,
    .row_size_bytes = 4,
    .info.format = GBitmapFormat4BitPalette,
    .bounds = GRect(0, 2, 8, 8),
    .palette = palette,
  };

  ResidentImage *image = resident_image_create_from_bitmap(&bitmap);
  cl_assert(image);
  cl_assert_equal_i(resident_image_get_size(image).h, 8);
  cl_assert_equal_i(resident_image_get_palette(image)[1].argb, 0xff);
  resident_image_draw(NULL, image, &GRect(0, 0, 8, 8));
  cl_assert_equal_m(&s_canvas[0][0], pixels + 8, 4);
  cl_assert_equal_m(&s_canvas[7][0], pixels + 36, 4);
  resident_image_destroy(image);
}

void test_resident_image__out_of_memory_frees_everything(void) {
  s_fail_after = 10;
  cl_assert(!resident_image_create(GSize(260, 260), GBitmapFormat4BitPalette, NULL, 0));
}
