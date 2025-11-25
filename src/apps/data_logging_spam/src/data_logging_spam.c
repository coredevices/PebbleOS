/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

static void log_data(void *data) {
  DataLoggingSessionRef *session = data_logging_create(0, DATA_LOGGING_BYTE_ARRAY, 4, true);

  for (int i = 0; i < 32; ++i) {
    uint32_t t = ((uint32_t) time(NULL)) + i;
    data_logging_log(session, &t, 1);
  }

  data_logging_finish(session);

  app_timer_register(100, log_data, 0);
}

int main(void) {
  Window *window = window_create();

  const bool animated = true;
  window_stack_push(window, animated);

  app_timer_register(100, log_data, 0);

  app_event_loop();
}

