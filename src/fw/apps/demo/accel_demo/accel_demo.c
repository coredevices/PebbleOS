/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "accel_demo.h"

#include "applib/accel_service.h"
#include "applib/app.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "util/size.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  Window rate_window;
  Window batch_window;
  Window data_window;
  SimpleMenuLayer rate_menu;
  SimpleMenuLayer batch_menu;
  TextLayer title_layer;
  TextLayer status_layer;
  TextLayer tap_layer;
  TextLayer data_layer;
  char title_buffer[24];
  char tap_buffer[40];
  char data_buffer[48];
  uint32_t tap_count;
  AccelSamplingRate rate;
  uint32_t batch;
  // When false, the live tap/data readouts are frozen (SELECT toggles it).
  bool display_on;
} AccelDemoAppData;

// Selectable options. Each menu item maps by index into these tables.
static const AccelSamplingRate s_rates[] = {
  ACCEL_SAMPLING_10HZ,
  ACCEL_SAMPLING_25HZ,
  ACCEL_SAMPLING_50HZ,
  ACCEL_SAMPLING_100HZ,
};

static const uint32_t s_batches[] = {1, 5, 10, 25};

static void prv_rate_selected(int index, void *context);
static void prv_batch_selected(int index, void *context);

static const SimpleMenuItem s_rate_items[] = {
  {.title = "10 Hz", .callback = prv_rate_selected},
  {.title = "25 Hz", .callback = prv_rate_selected},
  {.title = "50 Hz", .callback = prv_rate_selected},
  {.title = "100 Hz", .callback = prv_rate_selected},
};

static const SimpleMenuSection s_rate_sections[] = {
  {.title = "Sample rate", .items = s_rate_items, .num_items = ARRAY_LENGTH(s_rate_items)},
};

static const SimpleMenuItem s_batch_items[] = {
  {.title = "1 sample", .callback = prv_batch_selected},
  {.title = "5 samples", .callback = prv_batch_selected},
  {.title = "10 samples", .callback = prv_batch_selected},
  {.title = "25 samples", .callback = prv_batch_selected},
};

static const SimpleMenuSection s_batch_sections[] = {
  {.title = "Batched samples", .items = s_batch_items, .num_items = ARRAY_LENGTH(s_batch_items)},
};

static char prv_axis_char(AccelAxisType axis) {
  switch (axis) {
    case ACCEL_AXIS_X:
      return 'X';
    case ACCEL_AXIS_Y:
      return 'Y';
    case ACCEL_AXIS_Z:
      return 'Z';
    default:
      return '?';
  }
}

static void prv_update_status(AccelDemoAppData *data) {
  text_layer_set_text(&data->status_layer, data->display_on ? "Display: ON" : "Display: OFF");
}

static void prv_handle_tap(AccelAxisType axis, int32_t direction) {
  AccelDemoAppData *data = app_state_get_user_data();

  ++data->tap_count;
  if (!data->display_on) {
    return;
  }

  snprintf(data->tap_buffer, sizeof(data->tap_buffer), "Taps: %" PRIu32 "\n%c %c",
           data->tap_count, prv_axis_char(axis), direction < 0 ? '-' : '+');
  text_layer_set_text(&data->tap_layer, data->tap_buffer);
}

static void prv_handle_accel_data(AccelData *samples, uint32_t num_samples) {
  AccelDemoAppData *data = app_state_get_user_data();

  if (!data->display_on || num_samples == 0) {
    return;
  }

  // Show the most recent sample of the batch.
  const AccelData *latest = &samples[num_samples - 1];
  snprintf(data->data_buffer, sizeof(data->data_buffer), "X: %d\nY: %d\nZ: %d", latest->x,
           latest->y, latest->z);
  text_layer_set_text(&data->data_layer, data->data_buffer);
}

static void prv_toggle_click_handler(ClickRecognizerRef recognizer, void *context) {
  AccelDemoAppData *data = app_state_get_user_data();

  data->display_on = !data->display_on;
  prv_update_status(data);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_toggle_click_handler);
}

static void prv_data_window_load(Window *window) {
  AccelDemoAppData *data = app_state_get_user_data();
  Layer *root = &window->layer;
  const int16_t w = root->bounds.size.w;

  snprintf(data->title_buffer, sizeof(data->title_buffer), "%u Hz  x%" PRIu32,
           (unsigned)data->rate, data->batch);
  text_layer_init(&data->title_layer, &GRect(0, 0, w, 24));
  text_layer_set_text(&data->title_layer, data->title_buffer);
  layer_add_child(root, &data->title_layer.layer);

  text_layer_init(&data->status_layer, &GRect(0, 28, w, 20));
  layer_add_child(root, &data->status_layer.layer);
  prv_update_status(data);

  text_layer_init(&data->tap_layer, &GRect(0, 50, w, 44));
  text_layer_set_text(&data->tap_layer, "Taps: 0");
  layer_add_child(root, &data->tap_layer.layer);

  text_layer_init(&data->data_layer, &GRect(0, 96, w, 70));
  text_layer_set_text(&data->data_layer, "X: -\nY: -\nZ: -");
  layer_add_child(root, &data->data_layer.layer);

  // Tap (shake) events.
  accel_tap_service_subscribe(prv_handle_tap);

  // Accelerometer data stream at the selected rate, batched (no peek).
  accel_data_service_subscribe(data->batch, prv_handle_accel_data);
  accel_service_set_sampling_rate(data->rate);
}

static void prv_data_window_unload(Window *window) {
  accel_data_service_unsubscribe();
  accel_tap_service_unsubscribe();
}

static void prv_batch_window_load(Window *window) {
  AccelDemoAppData *data = app_state_get_user_data();
  simple_menu_layer_init(&data->batch_menu, &window->layer.bounds, window, s_batch_sections,
                         ARRAY_LENGTH(s_batch_sections), data);
  layer_add_child(&window->layer, simple_menu_layer_get_layer(&data->batch_menu));
}

static void prv_batch_window_unload(Window *window) {
  AccelDemoAppData *data = app_state_get_user_data();
  simple_menu_layer_deinit(&data->batch_menu);
}

static void prv_rate_window_load(Window *window) {
  AccelDemoAppData *data = app_state_get_user_data();
  simple_menu_layer_init(&data->rate_menu, &window->layer.bounds, window, s_rate_sections,
                         ARRAY_LENGTH(s_rate_sections), data);
  layer_add_child(&window->layer, simple_menu_layer_get_layer(&data->rate_menu));
}

static void prv_rate_window_unload(Window *window) {
  AccelDemoAppData *data = app_state_get_user_data();
  simple_menu_layer_deinit(&data->rate_menu);
}

static void prv_rate_selected(int index, void *context) {
  AccelDemoAppData *data = app_state_get_user_data();

  data->rate = s_rates[index];

  window_init(&data->batch_window, "Accel Demo Batch");
  window_set_window_handlers(&data->batch_window, &(WindowHandlers){
                                                      .load = prv_batch_window_load,
                                                      .unload = prv_batch_window_unload,
                                                  });
  app_window_stack_push(&data->batch_window, true /* animated */);
}

static void prv_batch_selected(int index, void *context) {
  AccelDemoAppData *data = app_state_get_user_data();

  data->batch = s_batches[index];

  window_init(&data->data_window, "Accel Demo Data");
  window_set_window_handlers(&data->data_window, &(WindowHandlers){
                                                     .load = prv_data_window_load,
                                                     .unload = prv_data_window_unload,
                                                 });
  window_set_click_config_provider(&data->data_window, prv_click_config_provider);
  app_window_stack_push(&data->data_window, true /* animated */);
}

static void prv_handle_init(void) {
  AccelDemoAppData *data = app_malloc_check(sizeof(AccelDemoAppData));
  memset(data, 0, sizeof(*data));
  data->display_on = true;
  data->rate = ACCEL_SAMPLING_25HZ;
  data->batch = 5;
  app_state_set_user_data(data);

  window_init(&data->rate_window, "Accel Demo Rate");
  window_set_window_handlers(&data->rate_window, &(WindowHandlers){
                                                     .load = prv_rate_window_load,
                                                     .unload = prv_rate_window_unload,
                                                 });
  app_window_stack_push(&data->rate_window, true /* animated */);
}

static void prv_handle_deinit(void) {
  AccelDemoAppData *data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  prv_handle_init();
  app_event_loop();
  prv_handle_deinit();
}

const PebbleProcessMd *accel_demo_get_info(void) {
  static const PebbleProcessMdSystem s_accel_demo_info = {
    .common.main_func = s_main,
    .name = "Accel Demo",
  };
  return (const PebbleProcessMd *)&s_accel_demo_info;
}
