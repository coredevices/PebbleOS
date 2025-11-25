/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

// Access profiler functions.
extern void __profiler_init(void);
extern void __profiler_print_stats(void);
extern void __profiler_start(void);
extern void __profiler_stop(void);

#define ITERATIONS 1000

static Window *window;

static uint16_t min(uint16_t a, uint16_t b) {
  return a < b ? a : b;
}

static void prv_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorRed);

  uint16_t radius = min(bounds.size.w / 2, bounds.size.h / 2);
  uint16_t inset_thickness = 1;

  // 360 degrees, filled in
  __profiler_start();
  for (int i = 0; i < ITERATIONS; ++i) {
    graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, radius, 0, TRIG_MAX_ANGLE);
  }
  __profiler_stop();
  APP_LOG(APP_LOG_LEVEL_INFO, "360 filled");
  __profiler_print_stats();

  // 360 degrees, 1px inset
  __profiler_start();
  for (int i = 0; i < ITERATIONS; ++i) {
    graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, inset_thickness, 0, TRIG_MAX_ANGLE);
  }
  __profiler_stop();
  APP_LOG(APP_LOG_LEVEL_INFO, "360 insets");
  __profiler_print_stats();

  // 180 degrees, filled in
  __profiler_start();
  for (int i = 0; i < ITERATIONS; ++i) {
    graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, radius, 0, TRIG_MAX_ANGLE / 2);
  }
  __profiler_stop();
  APP_LOG(APP_LOG_LEVEL_INFO, "180 filled");
  __profiler_print_stats();

  // 180 degrees, 1px inset
  __profiler_start();
  for (int i = 0; i < ITERATIONS; ++i) {
    graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, inset_thickness, 0,
                         TRIG_MAX_ANGLE / 2);
  }
  __profiler_stop();
  APP_LOG(APP_LOG_LEVEL_INFO, "180 insets");
  __profiler_print_stats();
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  layer_set_update_proc(window_layer, prv_update_proc);
}

static void init(void) {
  __profiler_init();
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
  });
  window_stack_push(window, true);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
