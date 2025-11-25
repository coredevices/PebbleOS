/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "progress_app.h"

#include <bluetooth/reconnect.h>

#include "applib/app.h"
#include "applib/fonts/fonts.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/ui.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"

static unsigned int s_progress_count = 0;

struct AppState {
  Window window;
};

static void prv_window_load(Window *window) {
  struct AppState* data = window_get_user_data(window);
  (void)data;
}

static void push_window(struct AppState *data) {
  Window* window = &data->window;
  window_init(window, WINDOW_NAME("Kill BT Demo"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}


////////////////////
// App boilerplate

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  PBL_LOG(LOG_LEVEL_DEBUG, "Try to kill the BT:%d", s_progress_count);
  s_progress_count += 1;

  bt_ctl_reset_bluetooth();
}

static void handle_init(void) {
  struct AppState* data = app_malloc_check(sizeof(struct AppState));

  app_state_set_user_data(data);
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
  push_window(data);
}

static void handle_deinit(void) {
  struct AppState* data = app_state_get_user_data();
  app_free(data);
}

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* kill_bt_app_get_info() {
  static const PebbleProcessMdSystem kill_bt_app_info = {
    .name = "Kill BT Test",
    .common.main_func = s_main,
  };
  return (const PebbleProcessMd*) &kill_bt_app_info;
}

