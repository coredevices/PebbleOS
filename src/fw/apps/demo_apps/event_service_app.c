/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "text_layout.h"

#include "applib/accel_service.h"
#include "applib/app.h"
#include "applib/connection_service.h"
#include "applib/fonts/fonts.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_management/app_manager.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  Window window;
  TextLayer count_layer;
  TextLayer connected_layer;
  char count_str[10];
  int count;
} EventServiceAppData;

static void handle_tap(AccelAxisType axis, int32_t sign) {
  EventServiceAppData *data = app_state_get_user_data();
  ++data->count;
  snprintf(data->count_str, 10, "%d", data->count);
  text_layer_set_text(&data->count_layer, data->count_str);
}

static void handle_bt_connection(bool connected) {
  EventServiceAppData *data = app_state_get_user_data();
  text_layer_set_text(&data->connected_layer, connected ? "connected" : "disconnected");
}

static void handle_deinit(void) {
  EventServiceAppData *data = app_state_get_user_data();
  app_free(data);

  accel_tap_service_unsubscribe();
  connection_service_unsubscribe();
}

static void handle_init(void) {
  EventServiceAppData *data = app_malloc_check(sizeof(EventServiceAppData));
  memset(data, 0, sizeof(EventServiceAppData));
  app_state_set_user_data(data);

  // Init window
  window_init(&data->window, "Event Service Demo");
  app_window_stack_push(&data->window, true /* Animated */);

  // Init text layer
  Layer *windowlayer = &data->window.layer;
  const int16_t width = windowlayer->bounds.size.w - ACTION_BAR_WIDTH - 6;
  text_layer_init(&data->count_layer, &GRect(0, 0, width, 20));
  layer_add_child(&data->window.layer, &data->count_layer.layer);
  text_layer_init(&data->connected_layer, &GRect(0, 20, width, 20));
  layer_add_child(&data->window.layer, &data->connected_layer.layer);

  text_layer_set_text(&data->count_layer, "No Presses");
  text_layer_set_text(&data->connected_layer, "No connection event");

  // subscribe to the accelerometer event stream
  accel_tap_service_subscribe(&handle_tap);
  ConnectionHandlers conn_handlers = {
    .pebble_app_connection_handler = handle_bt_connection
  };

  connection_service_subscribe(conn_handlers);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* event_service_app_get_info() {
  static const PebbleProcessMdSystem event_service_app_info = {
    .common.main_func = &s_main,
    .name = "Event Service App",
  };
  return (const PebbleProcessMd*) &event_service_app_info;
}
