/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

// Access profiler functions.
extern void __profiler_init(void);
extern void __profiler_print_stats(void);
extern void __profiler_start(void);
extern void __profiler_stop(void);

#define ITERATIONS 100

static Window *window;
static const char *TEXT = "Lorem ipsum dolor sit amet, consectetur adipiscing "\
                          "elit, sed do eiusmod tempor incididunt ut labore "\
                          "et dolore magna aliqua. Ut enim ad minim veniam, "\
                          "quis nostrud exercitation ullamco laboris nisi ut "\
                          "aliquip ex ea commodo consequat.";

static void prv_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_text_color(ctx, GColorBlack);

  __profiler_start();
  for (int i = 0; i < ITERATIONS; ++i) {
    graphics_draw_text(ctx, TEXT, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft,
                       NULL);
  }
  __profiler_stop();
  APP_LOG(APP_LOG_LEVEL_INFO, "Draw Text");
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
