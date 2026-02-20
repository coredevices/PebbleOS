/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_management/pebble_process_md.h"
#include "applib/app.h"
#include "applib/app_logging.h"
#include "process_state/app_state/app_state.h"
#include "applib/ui/ui.h"
#include "process_management/pebble_process_md.h"
#include "system/logging.h"
#include "system/passert.h"

static AppTimer *s_timer = NULL;

static void shouldnt_happen(void *context) {
  WTF;
}

static void stupid_cancel(void *context) {
  app_timer_cancel(s_timer);

  APP_LOG(LOG_LEVEL_INFO, "success");
}

static void prv_window_load(Window *window) {
  int dummy_data = 0;
  // Wait much longer than it should take to cancel the timer.
  AppTimer *timer = app_timer_register(1000 /*ms*/, shouldnt_happen, &dummy_data);
  PBL_ASSERTN(timer != NULL);
  // Try to cancel it twice.  This used to crash, but should not crash anymore.
  // In particular, we're looking to see that at least if we don't do more app_heap
  // allocations, we will be able to detect that we're effectively trying to
  // double-release this timer.
  app_timer_cancel(timer);
  app_timer_cancel(timer);

  timer = app_timer_register(1 /*ms*/, stupid_cancel, &s_timer);
  s_timer = timer;
  PBL_ASSERTN(timer != NULL);
}

static const WindowHandlers s_main_menu_handlers = {
  .load = prv_window_load,
};

static void handle_init(void) {
  Window *window = window_create();
  if (window == NULL) {
    return;
  }
  window_init(window, "");
  window_set_window_handlers(window, &s_main_menu_handlers);
  app_window_stack_push(window, true /*animated*/);
}

static void handle_deinit(void) {
  // Don't bother freeing anything, the OS should be re-initing the heap.
}

static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

const PebbleProcessMd* timer_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "Timer Cancel Test"
  };
  return (const PebbleProcessMd*) &s_app_info;
}
