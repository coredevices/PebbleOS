/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/tick_timer_service.h"
#include "util/trig.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "applib/ui/window_private.h"
#include "applib/ui/path_layer.h"
#include "applib/ui/text_layer.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "drivers/accel.h"
#include "process_state/app_state/app_state.h"
#include "process_management/pebble_process_md.h"
#include "services/common/evented_timer.h"
#include "util/bitset.h"
#include "util/size.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define STATUS_STRING_LEN 50

static EventedTimerID s_timer;

typedef struct {
  Window window;

  TextLayer title;
  TextLayer status;
  char status_string[STATUS_STRING_LEN];
} AppData;

static void prv_update_display(void *context) {
  AppData *data = context;

  AccelDriverSample sample;

  int ret = accel_peek(&sample);

  if (ret == 0) {
    sniprintf(data->status_string, sizeof(data->status_string),
              "X: %"PRIi16"\nY: %"PRIi16"\nZ:%"PRIi16"", sample.x, sample.y, sample.z);
  } else {
    sniprintf(data->status_string, sizeof(data->status_string),
              "ACCEL ERROR:\n%d", ret);
  }

  text_layer_set_text(&data->status, data->status_string);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {};

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.bounds);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "ACCEL TEST");
  layer_add_child(&window->layer, &title->layer);

  TextLayer *status = &data->status;
  text_layer_init(status,
                  &GRect(5, 40,
                         window->layer.bounds.size.w - 5, window->layer.bounds.size.h - 40));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  layer_add_child(&window->layer, &status->layer);

  app_window_stack_push(window, true /* Animated */);

  s_timer = evented_timer_register(100, true /* repeating */, prv_update_display, data);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();

  evented_timer_cancel(s_timer);
}

const PebbleProcessMd* mfg_accel_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: ED2E214A-D4B5-4360-B5EC-612B9E49FB95
    .common.uuid = { 0xED, 0x2E, 0x21, 0x4A, 0xD4, 0xB5, 0x43, 0x60,
                     0xB5, 0xEC, 0x61, 0x2B, 0x9E, 0x49, 0xFB, 0x95,
    },
    .name = "MfgAccel",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
