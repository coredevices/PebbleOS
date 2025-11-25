/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/app_light.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/inverter_layer.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "applib/tick_timer_service.h"
#include "util/size.h"

//! How long to wait between vibes
static int INTER_VIBE_PERIOD_MS = 5000;

typedef struct {
  Window window;

  TextLayer title;

  InverterLayer inverter;
} AppData;

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  Layer *inverter = &data->inverter.layer;
  layer_set_hidden(inverter, !layer_get_hidden(inverter));
}

static void prv_vibe_timer_callback(void *data) {
  static const uint32_t SECOND_PULSE_DURATIONS[] = { 1000 };
  VibePattern pat = {
    .durations = SECOND_PULSE_DURATIONS,
    .num_segments = ARRAY_LENGTH(SECOND_PULSE_DURATIONS)
  };
  vibes_enqueue_custom_pattern(pat);

  app_timer_register(INTER_VIBE_PERIOD_MS, prv_vibe_timer_callback, NULL);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);

  TextLayer *title = &data->title;
  text_layer_init(title, &window->layer.frame);
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "Certification");
  layer_add_child(&window->layer, &title->layer);

  InverterLayer *inverter = &data->inverter;
  inverter_layer_init(inverter, &window->layer.frame);
  layer_add_child(&window->layer, &inverter->layer);

  app_window_stack_push(window, true /* Animated */);

  prv_vibe_timer_callback(NULL);
  app_light_enable(true);
  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_second_tick);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd* mfg_certification_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 266135d1-827f-4f64-9752-fffe604e1dbe
    .common.uuid = { 0x26, 0x61, 0x35, 0xd1, 0x82, 0x7f, 0x4f, 0x64,
                     0x97, 0x52, 0xff, 0xfe, 0x60, 0x4e, 0x1d, 0xbe },
    .name = "MfgCertification",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
