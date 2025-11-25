/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>
#include <inttypes.h>

#define ACCEL_RAW_DATA 0
#define TIMEOUT_MS 1000

Window *window;
TextLayer *text_layer;

AppTimer* s_timer;
static AccelData s_last_accel_data;

static uint32_t prv_compute_delta_pos(AccelData *cur_pos, AccelData *last_pos) {
  return (abs(last_pos->x - cur_pos->x) + abs(last_pos->y - cur_pos->y) +
      abs(last_pos->z - cur_pos->z));
}

static void prv_timer_cb(void *data) {
  s_timer = app_timer_register(TIMEOUT_MS, prv_timer_cb, NULL);

  AccelData accel_data;
  int error;
  if ((error = accel_service_peek(&accel_data)) != 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Accelerometer error %i", error);
    return;
  }

  static char accel_text[20];
#if !ACCEL_RAW_DATA
  int32_t delta = prv_compute_delta_pos(&accel_data, &s_last_accel_data);
  s_last_accel_data = accel_data;

  snprintf(accel_text, sizeof(accel_text), "Accel delta: %"PRIu32, delta);
  APP_LOG(APP_LOG_LEVEL_INFO, accel_text);
#else
  snprintf(accel_text, sizeof(accel_text), "x:%"PRId16 ", y:%"PRId16 ", z:%"PRId16,
      accel_data.x, accel_data.y, accel_data.z);
  APP_LOG(APP_LOG_LEVEL_INFO, accel_text);
#endif
  text_layer_set_text(text_layer, accel_text);
}

void handle_init(void) {
  // Create a window and text layer
  window = window_create();
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  uint32_t text_width = bounds.size.w;
  uint32_t text_height = 28;
  text_layer = text_layer_create(GRect(0, bounds.size.h/2 - text_height/2,
        text_width, text_height));

  // Set the text, font, and text alignment
  text_layer_set_text(text_layer, "No Accelerometer");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);

  // Add the text layer to the window
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_layer));

  // Push the window
  window_stack_push(window, true);

  // App Logging!
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Just pushed a window!");

  // Subscribe Accelerometer and Register Timer
  accel_data_service_subscribe(0, NULL);
  s_timer = app_timer_register(TIMEOUT_MS, prv_timer_cb, NULL);
}

void handle_deinit(void) {
  // Destroy Timer and Unsubscribe Accelerometer
  app_timer_cancel(s_timer);
  accel_data_service_unsubscribe();

  // Destroy the text layer
  text_layer_destroy(text_layer);

  // Destroy the window
  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
