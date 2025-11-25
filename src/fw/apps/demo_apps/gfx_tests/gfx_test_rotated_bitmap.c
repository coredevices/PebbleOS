/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gfx_tests.h"
#include "util/trig.h"

static GBitmap *s_gfx_rotated_bitmap_bitmap;
static GBitmap *s_gfx_rotated_bitmap_64_bitmap;

static GPoint bitmap_center = {DISP_COLS / 2, DISP_ROWS / 2};
static GPoint bitmap_64_center = {64 / 2, 64 / 2};

static void prv_setup(Window *window) {
#if SCREEN_COLOR_DEPTH_BITS == 1
  s_gfx_rotated_bitmap_bitmap = gbitmap_create_blank(GSize(DISP_COLS, DISP_ROWS),
                                                     GBitmapFormat1Bit);
  s_gfx_rotated_bitmap_64_bitmap = gbitmap_create_blank(GSize(64, 64), GBitmapFormat1Bit);
  uint32_t size_full = (DISP_COLS*DISP_ROWS)/8;
#else
  s_gfx_rotated_bitmap_bitmap = gbitmap_create_blank(GSize(DISP_COLS, DISP_ROWS),
                                                     GBitmapFormat8Bit);
  s_gfx_rotated_bitmap_64_bitmap = gbitmap_create_blank(GSize(64, 64), GBitmapFormat8Bit);
  uint32_t size_full = (DISP_COLS*DISP_ROWS);
#endif
  uint8_t *s_gfx_rotated_bitmap_pixels = s_gfx_rotated_bitmap_bitmap->addr;
  // Init images
  for (uint32_t index = 0; index < size_full; index++) {
    s_gfx_rotated_bitmap_pixels[index] = (index % 2) ? 0xf0 : 0xcc;
  }

#if SCREEN_COLOR_DEPTH_BITS == 1
  uint32_t size_64 = (64*64)/8;
#else
  uint32_t size_64 = (64*64);
#endif
  uint8_t *s_gfx_rotated_bitmap_64_pixels = s_gfx_rotated_bitmap_64_bitmap->addr;
  for (uint32_t index = 0; index < size_64; index++) {
    s_gfx_rotated_bitmap_64_pixels[index] = (index % 2) ? 0xf0 : 0xcc;
  }
}

static void prv_test_0_assign(Layer *layer, GContext* ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_draw_rotated_bitmap(ctx, (GBitmap*)&s_gfx_rotated_bitmap_bitmap,
                               GPointZero, 0, GPointZero);
}

static void prv_test_0_set(Layer *layer, GContext* ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_rotated_bitmap(ctx, (GBitmap*)&s_gfx_rotated_bitmap_bitmap,
                               GPointZero, 0, GPointZero);
}

static void prv_test_45_assign(Layer *layer, GContext* ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_draw_rotated_bitmap(ctx, (GBitmap*)&s_gfx_rotated_bitmap_bitmap,
                               bitmap_center, DEG_TO_TRIGANGLE(45), bitmap_center);
}

static void prv_test_45_set(Layer *layer, GContext* ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_rotated_bitmap(ctx, (GBitmap*)&s_gfx_rotated_bitmap_bitmap,
                               bitmap_center, DEG_TO_TRIGANGLE(45), bitmap_center);
}

static void prv_test_0_assign_64px(Layer *layer, GContext* ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_draw_rotated_bitmap(ctx, (GBitmap*)&s_gfx_rotated_bitmap_64_bitmap,
                               GPointZero, 0, GPointZero);
}

static void prv_test_0_set_64px(Layer *layer, GContext* ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_rotated_bitmap(ctx, (GBitmap*)&s_gfx_rotated_bitmap_64_bitmap,
                               GPointZero, 0, GPointZero);
}

static void prv_test_45_assign_64px(Layer *layer, GContext* ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_draw_rotated_bitmap(ctx, (GBitmap*)&s_gfx_rotated_bitmap_64_bitmap,
                               bitmap_64_center, DEG_TO_TRIGANGLE(45), bitmap_64_center);
}

static void prv_test_45_set_64px(Layer *layer, GContext* ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_rotated_bitmap(ctx, (GBitmap*)&s_gfx_rotated_bitmap_64_bitmap,
                               bitmap_64_center, DEG_TO_TRIGANGLE(45), bitmap_64_center);
}

static void prv_teardown(Window *window) {
  gbitmap_destroy(s_gfx_rotated_bitmap_bitmap);
  gbitmap_destroy(s_gfx_rotated_bitmap_64_bitmap);
}

GfxTest g_gfx_test_rotated_bitmap_0_assign = {
  .name = "RotBit 0-A-full",
  .duration = 5,
  .unit_multiple = 1,
  .test_proc = prv_test_0_assign,
  .setup = prv_setup,
  .teardown = prv_teardown,
};

GfxTest g_gfx_test_rotated_bitmap_0_set = {
  .name = "RotBit 0-S-full",
  .duration = 5,
  .unit_multiple = 1,
  .test_proc = prv_test_0_set,
  .setup = prv_setup,
  .teardown = prv_teardown,
};

GfxTest g_gfx_test_rotated_bitmap_45_assign = {
  .name = "RotBit-45-A-full",
  .duration = 5,
  .unit_multiple = 1,
  .test_proc = prv_test_45_assign,
  .setup = prv_setup,
  .teardown = prv_teardown,
};

GfxTest g_gfx_test_rotated_bitmap_45_set = {
  .name = "RotBit-45-S-full",
  .duration = 5,
  .unit_multiple = 1,
  .test_proc = prv_test_45_set,
  .setup = prv_setup,
  .teardown = prv_teardown,
};

GfxTest g_gfx_test_rotated_bitmap_0_assign_64px = {
  .name = "RotBit-0-A-64px",
  .duration = 5,
  .unit_multiple = 1,
  .test_proc = prv_test_0_assign_64px,
  .setup = prv_setup,
  .teardown = prv_teardown,
};

GfxTest g_gfx_test_rotated_bitmap_0_set_64px = {
  .name = "RotBit-0-S-64px",
  .duration = 5,
  .unit_multiple = 1,
  .test_proc = prv_test_0_set_64px,
  .setup = prv_setup,
  .teardown = prv_teardown,
};

GfxTest g_gfx_test_rotated_bitmap_45_assign_64px = {
  .name = "RotBit-45-A-64px",
  .duration = 5,
  .unit_multiple = 1,
  .test_proc = prv_test_45_assign_64px,
  .setup = prv_setup,
  .teardown = prv_teardown,
};

GfxTest g_gfx_test_rotated_bitmap_45_set_64px = {
  .name = "RotBit-45-S-64px",
  .duration = 5,
  .unit_multiple = 1,
  .test_proc = prv_test_45_set_64px,
  .setup = prv_setup,
  .teardown = prv_teardown,
};
