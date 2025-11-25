/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gfx_tests.h"

static void prv_test(Layer *layer, GContext* ctx);

GfxTest g_gfx_test_single_line = {
  .name = "Single line",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test,
};

static void prv_test(Layer *layer, GContext* ctx) {
  GRect bounds = layer->bounds;
  int16_t x1 = (rand() % bounds.size.w) + bounds.origin.x;
  int16_t x2 = (rand() % bounds.size.w) + bounds.origin.x;
  int16_t y1 = (rand() % bounds.size.h) + bounds.origin.y;
  int16_t y2 = (rand() % bounds.size.h) + bounds.origin.y;

  GColor color = { .argb = (uint8_t) rand() };
  graphics_context_set_stroke_color(ctx, color);
  graphics_draw_line(ctx, (GPoint){.x = x1, .y = y1}, (GPoint) {.x = x2, .y = y2});
}
