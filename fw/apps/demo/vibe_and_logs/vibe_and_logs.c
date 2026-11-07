/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/vibe.h>
#include <pbl/logging/logging.h>

#include <applib/app.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/vibes.h>
#include <applib/ui/window_stack.h>
#include <kernel/events.h>
#include <process_state/app_state/app_state.h>
#include <trigger_alarm.h>

Window s_window;
static AppTimer *s_app_timer;

static void app_timer_callback(void *data) {
  for (int i = 0; i < 40; i++) {
    PBL_LOG_INFO("%d Running app timer callback", i);
    vibes_short_pulse();
  }
  s_app_timer = app_timer_register(100 /* milliseconds */, app_timer_callback, nullptr);
}

static void handle_init(void) {
  window_init(&s_window, WINDOW_NAME("VibeAndLogs Demo"));
  const bool animated = true;
  app_window_stack_push(&s_window, animated);

  s_app_timer = app_timer_register(100 /* milliseconds */, app_timer_callback, nullptr);
}

static void handle_deinit(void) {
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd *vibe_and_logs_get_app_info() {
  static const PebbleProcessMdSystem s_trigger_alarm = {
    .common.main_func = s_main,
    .name = "VibeAndLogs"
  };

  return (const PebbleProcessMd *)&s_trigger_alarm;
}
