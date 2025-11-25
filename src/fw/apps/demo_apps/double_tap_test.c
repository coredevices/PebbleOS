/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "double_tap_test.h"

#include <stdio.h>

#include "applib/accel_service.h"
#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "resource/system_resource.h"
#include "system/logging.h"
#include "kernel/pbl_malloc.h"

typedef struct {
  Window window;
  TextLayer thumbsup_layer;
  TextLayer text_layer;
  char text[40];
  uint32_t count;
  AppTimer *thumbsup_timer;
} AppData;

static void prv_hide_thumbsup(void *ctx) {
  AppData *data = ctx;
  text_layer_set_text(&data->thumbsup_layer, "");
}

static void prv_show_thumbsup(AppData *data) {
  app_timer_cancel(data->thumbsup_timer);
  text_layer_set_text(&data->thumbsup_layer, "👍");
  data->thumbsup_timer = app_timer_register(1000, prv_hide_thumbsup, data);
}

static void prv_set_tap_text(AppData *data, uint32_t count, AccelAxisType axis) {
  char axes[] = {'X', 'Y', 'Z'};
  snprintf(data->text, sizeof(data->text), "Axis: %c\nDouble Taps: %6"PRIu32, axes[axis], count);
  text_layer_set_text(&data->text_layer, data->text);
}

static void prv_window_load(Window *window) {
  AppData *data = window_get_user_data(window);

  GSize size = window->layer.frame.size;

  TextLayer *thumbsup_layer = &data->thumbsup_layer;
  text_layer_init(thumbsup_layer, &(GRect){{0, 1 * (size.h / 3)}, {size.w, 50}});
  text_layer_set_font(thumbsup_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(thumbsup_layer, GTextAlignmentCenter);
  layer_add_child(&window->layer, (Layer *)thumbsup_layer);

  TextLayer *text_layer = &data->text_layer;
  text_layer_init(text_layer, &(GRect){{0, 2 * (size.h / 3)}, {size.w, 40}});
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  data->count = 0;
  prv_set_tap_text(data, 0, 0);
  layer_add_child(&window->layer, (Layer *)text_layer);
}

static void prv_handle_tap(AccelAxisType axis, int32_t direction) {
  AppData *data = app_state_get_user_data();
  prv_set_tap_text(data, ++data->count, axis);
  prv_show_thumbsup(data);
}

static void prv_handle_init(void) {
  AppData *data = (AppData*) app_malloc(sizeof(AppData));

  app_state_set_user_data(data);

  window_init(&data->window, WINDOW_NAME("Double Tap Test"));
  window_set_user_data(&data->window, data);
  window_set_window_handlers(&data->window, &(WindowHandlers) {
      .load = prv_window_load });

  const bool animated = true;
  app_window_stack_push(&data->window, animated);

  accel_double_tap_service_subscribe(prv_handle_tap);
}

static void prv_handle_deinit(void) {
  AppData *data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();

  prv_handle_deinit();
}

const PebbleProcessMd* double_tap_test_get_info() {
  static const PebbleProcessMdSystem s_accel_config_info = {
    .common.main_func = s_main,
    .name = "Double Tap Test"
  };
  return (const PebbleProcessMd*) &s_accel_config_info;
}
