/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_display_burnin.h"

#include "applib/app.h"
#include "applib/app_timer.h"
#include "applib/ui/app_window_stack.h"
#include "process_state/app_state/app_state.h"

#include "drivers/display/display.h"
#include "kernel/pbl_malloc.h"
#include "util/units.h"

#include <stddef.h>
#include <string.h>

typedef struct {
  Window window;
  Layer background;
  InverterLayer inverter_layer;
  uint32_t old_display_hz;
} MfgDisplayBurninAppData;

static void draw_checkerboard(Layer* background, GContext* c) {
  const int height = background->bounds.size.h;
  const int width = background->bounds.size.w;
  for(int y = 0; y < height; y += 4) {
    for(int x = 0; x < width; x += 4) {
      graphics_context_set_stroke_color(c, GColorBlack);
      graphics_draw_pixel(c, GPoint(x,y));
      graphics_draw_pixel(c, GPoint(x+1,y));
      graphics_draw_pixel(c, GPoint(x,y+1));
      graphics_draw_pixel(c, GPoint(x+1,y+1));
      graphics_draw_pixel(c, GPoint(x+2,y+2));
      graphics_draw_pixel(c, GPoint(x+3,y+2));
      graphics_draw_pixel(c, GPoint(x+2,y+3));
      graphics_draw_pixel(c, GPoint(x+3,y+3));
    }
  }
}


static void handle_timer(void *timer_data) {
  MfgDisplayBurninAppData *data = app_state_get_user_data();

  layer_set_hidden((Layer*)&data->inverter_layer,
      !layer_get_hidden((Layer*)&data->inverter_layer));
  app_timer_register(100 /* milliseconds */, handle_timer, NULL);
}

static void handle_init(void) {
  MfgDisplayBurninAppData *data = task_malloc_check(sizeof(MfgDisplayBurninAppData));

  app_state_set_user_data(data);

  // Overclock the display to 4MHz make the artifacts issue more likely to happen
  data->old_display_hz = display_baud_rate_change(MHZ_TO_HZ(4));

  window_init(&data->window, "Display Burnin");
  window_set_fullscreen(&data->window, true);
  app_window_stack_push(&data->window, true /* Animated */);

  layer_init(&data->background, &data->window.layer.bounds);
  data->background.update_proc = (LayerUpdateProc) draw_checkerboard;
  layer_add_child(&data->window.layer, (Layer*) &data->background);

  inverter_layer_init(&data->inverter_layer, &data->window.layer.bounds);
  layer_add_child(&data->window.layer, (Layer*) &data->inverter_layer);

  app_timer_register(100 /* milliseconds */, handle_timer, NULL);
}

static void handle_deinit(void) {
  MfgDisplayBurninAppData *data = app_state_get_user_data();

  display_baud_rate_change(data->old_display_hz);

  task_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

static const PebbleProcessMdSystem s_mfg_func_test = {
  .common = {
    // UUID: 1bef4e93-5ec4-4af8-9eff-196eaf25b92b
    .uuid = {0x1b, 0xef, 0x4e, 0x93, 0x5e, 0xc4, 0x4a, 0xf8, 0x9e, 0xff, 0x19, 0x6e, 0xaf, 0x25, 0xb9, 0x2b},
    .main_func = s_main
  },
  .name = "Display Burn-in"
};

const Uuid* mfg_display_burnin_get_uuid() {
  return &s_mfg_func_test.common.uuid;
}

const PebbleProcessMd* mfg_display_burnin_get_app_info() {
  return (const PebbleProcessMd*) &s_mfg_func_test;
}
