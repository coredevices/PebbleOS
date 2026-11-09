/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "8bit/test_framebuffer.h"
#include "graphics_common_stubs.h"
#include "test_graphics.h"
#include "util.h"

#include <stdlib.h>

#include <pbl/util/trig.h>

#include <applib/graphics/bitblt.h>
#include <applib/graphics/framebuffer.h>
#include <applib/graphics/graphics.h>
#include <applib/ui/layer.h>
#include <applib/ui/window_private.h>
#include <clar.h>

#define SRC_SIZE 64

static GContext s_ctx;
static FrameBuffer *s_fb;
static GBitmap *s_src;

// Each row and column of the screen tells apart what each path may get wrong
static void prv_fill_background(void) {
  uint8_t *fb = (uint8_t *)s_fb->buffer;
  for (int y = 0; y < PBL_DISPLAY_HEIGHT; y++) {
    for (int x = 0; x < PBL_DISPLAY_WIDTH; x++) {
      fb[y * PBL_DISPLAY_WIDTH + x] =
          (uint8_t)(0xC0U | ((x / 8) & 3U) << 4 | ((y / 8) & 3U) << 2 | ((x + y) & 3U));
    }
  }
}

void test_graphics_fast_paths__initialize(void) {
  s_fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(s_fb, &(GSize){PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT});
  test_graphics_context_init(&s_ctx, s_fb);
  framebuffer_clear(s_fb);
  prv_fill_background();

  s_src = gbitmap_create_blank(GSize(SRC_SIZE, SRC_SIZE), GBitmapFormat8Bit);
  uint8_t *src = gbitmap_get_data(s_src);
  for (int y = 0; y < SRC_SIZE; y++) {
    for (int x = 0; x < SRC_SIZE; x++) {
      // Quadrants of each alpha, with a gradient that shows any shift by a pixel
      const uint8_t alpha = (uint8_t)((x * 2 / SRC_SIZE) | ((y * 2 / SRC_SIZE) << 1));
      src[y * SRC_SIZE + x] =
          (uint8_t)((alpha << 6) | ((x & 3U) << 4) | ((y & 3U) << 2) | (((x + y) / 4) & 3U));
    }
  }
}

void test_graphics_fast_paths__cleanup(void) {
  gbitmap_destroy(s_src);
  free(s_fb);
}

static void prv_rotated(GCompOp op, int degrees, GPoint src_ic, GPoint dest_ic) {
  graphics_context_set_compositing_mode(&s_ctx, op);
  graphics_draw_rotated_bitmap(&s_ctx, s_src, src_ic, DEG_TO_TRIGANGLE(degrees), dest_ic);
}

void test_graphics_fast_paths__rotated_set(void) {
  prv_rotated(GCompOpSet, 33, GPoint(SRC_SIZE / 2, SRC_SIZE / 2), GPoint(70, 70));
  // Pivot outside the bitmap, so that both signs of each source offset occur
  prv_rotated(GCompOpSet, 210, GPoint(-10, 80), GPoint(100, 130));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_fast_paths__rotated_assign(void) {
  prv_rotated(GCompOpAssign, 77, GPoint(0, 0), GPoint(120, 40));
  prv_rotated(GCompOpAssign, 301, GPoint(SRC_SIZE - 1, SRC_SIZE - 1), GPoint(60, 150));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_fast_paths__rotated_clipped(void) {
  s_ctx.draw_state.clip_box = GRect(30, 40, 90, 70);
  prv_rotated(GCompOpSet, 45, GPoint(SRC_SIZE / 2, SRC_SIZE / 2), GPoint(70, 70));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_fast_paths__blit_assign(void) {
  graphics_context_set_compositing_mode(&s_ctx, GCompOpAssign);
  // Every alignment of the destination, and rows short and long enough for each loop
  for (int i = 0; i < 8; i++) {
    graphics_draw_bitmap_in_rect(&s_ctx, s_src, &GRect(4 + i, 8 + i * 9, 3 + i * 7, 8));
  }
  // Clipped on the left, and tiled past the end of the source
  graphics_draw_bitmap_in_rect(&s_ctx, s_src, &GRect(-5, 90, 40, 20));
  graphics_draw_bitmap_in_rect(&s_ctx, s_src, &GRect(50, 90, SRC_SIZE * 2 + 3, SRC_SIZE + 5));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_fast_paths__blit_assign_overlap(void) {
  // The source being the screen itself: rows repeat as they would byte by byte
  GBitmap self = s_ctx.dest_bitmap;
  self.info.format = GBitmapFormat8Bit;
  self.row_size_bytes = PBL_DISPLAY_WIDTH;
  const GRect rect = GRect(12, 40, 100, 30);
  bitblt_bitmap_into_bitmap_tiled(&s_ctx.dest_bitmap, &self, rect, GPoint(8, 40), GCompOpAssign,
                                  GColorWhite);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_graphics_fast_paths__fill_rect(void) {
  graphics_context_set_fill_color(&s_ctx, GColorRed);
  for (int i = 0; i < 8; i++) {
    graphics_fill_rect(&s_ctx, &GRect(i, 10 + i * 6, 1 + i * 5, 4));
  }
  graphics_context_set_fill_color(&s_ctx, GColorBlue);
  // Past every edge of the screen, which a round display masks row by row
  graphics_fill_rect(&s_ctx, &GRect(-10, 70, PBL_DISPLAY_WIDTH + 20, 30));
  graphics_fill_rect(&s_ctx, &GRect(60, -5, 21, PBL_DISPLAY_HEIGHT + 10));
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
