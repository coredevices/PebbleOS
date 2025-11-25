/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gfx_tests.h"

static GRect rect;
static GPoint center;
static GOvalScaleMode scale_mode;
static int inset;
static int outer_size;
static int inner_size;
static int32_t angle_start;
static int32_t angle_end;

static void prv_setup_data(GRect bounds) {
  center = GPoint(bounds.origin.x + (bounds.size.w / 2),
                  bounds.origin.y + (bounds.size.h / 2));
  rect = GRect(center.x - (outer_size / 2), center.y - (outer_size / 2),
               outer_size, outer_size);
  inset = outer_size - inner_size;
  scale_mode = GOvalScaleModeFitCircle;
}

static void prv_setup_even_angles_inner(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = window_layer->bounds;

  // Parameters
  outer_size = 50;
  inner_size = 35;
  angle_start = (TRIG_MAX_ANGLE / 8) * 3;
  angle_end = TRIG_MAX_ANGLE + TRIG_MAX_ANGLE / 8;

  prv_setup_data(bounds);
}

static void prv_setup_odd_angles_inner(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = window_layer->bounds;

  // Parameters
  outer_size = 49;
  inner_size = 34;
  angle_start = (TRIG_MAX_ANGLE / 8) * 3;
  angle_end = TRIG_MAX_ANGLE + TRIG_MAX_ANGLE / 8;

  prv_setup_data(bounds);
}

static void prv_setup_even_inner(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = window_layer->bounds;

  // Parameters
  outer_size = 50;
  inner_size = 35;
  angle_start = TRIG_MAX_ANGLE / 4;
  angle_end = TRIG_MAX_ANGLE;

  prv_setup_data(bounds);
}

static void prv_setup_odd_inner(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = window_layer->bounds;

  // Parameters
  outer_size = 49;
  inner_size = 34;
  angle_start = TRIG_MAX_ANGLE / 4;
  angle_end = TRIG_MAX_ANGLE;

  prv_setup_data(bounds);
}

static void prv_setup_even_angles_full(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = window_layer->bounds;

  // Parameters
  outer_size = 50;
  inner_size = 0;
  angle_start = (TRIG_MAX_ANGLE / 8) * 3;
  angle_end = TRIG_MAX_ANGLE + TRIG_MAX_ANGLE / 8;

  prv_setup_data(bounds);
}

static void prv_setup_odd_angles_full(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = window_layer->bounds;

  // Parameters
  outer_size = 49;
  inner_size = 0;
  angle_start = (TRIG_MAX_ANGLE / 8) * 3;
  angle_end = TRIG_MAX_ANGLE + TRIG_MAX_ANGLE / 8;

  prv_setup_data(bounds);
}

static void prv_setup_even_full(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = window_layer->bounds;

  // Parameters
  outer_size = 50;
  inner_size = 0;
  angle_start = TRIG_MAX_ANGLE / 4;
  angle_end = TRIG_MAX_ANGLE;

  prv_setup_data(bounds);
}

static void prv_setup_odd_full(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = window_layer->bounds;

  // Parameters
  outer_size = 49;
  inner_size = 0;
  angle_start = TRIG_MAX_ANGLE / 4;
  angle_end = TRIG_MAX_ANGLE;

  prv_setup_data(bounds);
}

static void prv_test_radial(Layer *layer, GContext* ctx) {
  GColor color = { .argb = (uint8_t) rand() };
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_radial(ctx, rect, GOvalScaleModeFillCircle, inset, angle_start, angle_end);
}

static void prv_test_circle(Layer *layer, GContext* ctx) {
  GColor color = { .argb = (uint8_t) rand() };
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, center, (outer_size / 2));
}

GfxTest g_gfx_test_annulus_even_fill_angles = {
  .name = "Annulus Even Angles",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_radial,
  .setup = prv_setup_even_angles_inner,
};

GfxTest g_gfx_test_annulus_odd_fill_angles = {
  .name = "Annulus Odd Angles",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_radial,
  .setup = prv_setup_odd_angles_inner,
};

GfxTest g_gfx_test_annulus_even_fill = {
  .name = "Annulus Even",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_radial,
  .setup = prv_setup_even_inner,
};

GfxTest g_gfx_test_annulus_odd_fill = {
  .name = "Annulus Odd",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_radial,
  .setup = prv_setup_odd_inner,
};

GfxTest g_gfx_test_radial_even_fill_angles = {
  .name = "Radial Even Angles",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_radial,
  .setup = prv_setup_even_angles_full,
};

GfxTest g_gfx_test_radial_odd_fill_angles = {
  .name = "Radial Odd Angles",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_radial,
  .setup = prv_setup_odd_angles_full,
};

GfxTest g_gfx_test_radial_even_fill = {
  .name = "Radial Even",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_radial,
  .setup = prv_setup_even_full,
};

GfxTest g_gfx_test_radial_odd_fill = {
  .name = "Radial Odd",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_radial,
  .setup = prv_setup_odd_full,
};

GfxTest g_gfx_test_circle_even = {
  .name = "Circle Even",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_circle,
  .setup = prv_setup_even_full,
};

GfxTest g_gfx_test_circle_odd = {
  .name = "Circle Odd",
  .duration = 1,
  .unit_multiple = 1,
  .test_proc = prv_test_circle,
  .setup = prv_setup_odd_full,
};
