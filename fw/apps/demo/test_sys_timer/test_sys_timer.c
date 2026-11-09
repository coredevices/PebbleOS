/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#undef CONFIG_LOG_HASHED

#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <applib/app.h>
#include <applib/app_malloc.h>
#include <applib/ui/ui.h>
#include <process_management/pebble_process_md.h>
#include <process_state/app_state/app_state.h>
#include <syscall/syscall.h>

#define NUM_MAX_TIMERS 10

// =================================================================================
// Application Data
typedef struct {
  Window *window;
  SimpleMenuLayer *menu_layer;

  uint64_t fired_time[NUM_MAX_TIMERS];

  struct pbl_wallclock_work reg_timers[NUM_MAX_TIMERS];

  AppTimer *app_timer;

} TestTimersAppData;

static TestTimersAppData *s_app_data = 0;

// =================================================================================
static void timer_callback(struct pbl_work *work);
static void long_timer_callback(struct pbl_work *work);
static void repeating_timer_callback(struct pbl_work *work);
static void stuck_timer_callback(struct pbl_work *work);

// Static, as they must outlive the app's heap if they are still pending when it exits.
static struct pbl_delayed_work s_timers[2] = {
  PBL_DELAYED_WORK_INITIALIZER(timer_callback),
  PBL_DELAYED_WORK_INITIALIZER(timer_callback),
};
static PBL_DELAYED_WORK_DEFINE(s_long_timer, long_timer_callback);
static PBL_DELAYED_WORK_DEFINE(s_repeating_timer, repeating_timer_callback);
static PBL_DELAYED_WORK_DEFINE(s_stuck_timer, stuck_timer_callback);

#define REPEATING_TIMER_PERIOD_MS 500

// =================================================================================
static void timer_callback(struct pbl_work *work) {
  int idx = container_of(work, struct pbl_delayed_work, work) - s_timers;
  PBL_ASSERTN(idx >= 0 && idx < NUM_MAX_TIMERS);
  PBL_LOG_DBG("STT normal callback %d executed", idx);
  s_app_data->fired_time[idx] = pbl_uptime_ticks();
  return;
}

// =================================================================================
static void repeating_timer_callback(struct pbl_work *work) {
  PBL_LOG_DBG("STT repeating callback executed");
  s_app_data->fired_time[0] = pbl_uptime_ticks();
  pbl_delayed_work_schedule_next(&s_repeating_timer, pbl_ms_to_ticks(REPEATING_TIMER_PERIOD_MS));
}

// =================================================================================
static void stuck_timer_callback(struct pbl_work *work) {
  PBL_LOG_DBG("STT entering infinite loop in callback");
  while (true) {
    sys_psleep(100);
  }
}

// =================================================================================
static void long_timer_callback(struct pbl_work *work) {
  PBL_LOG_DBG("STT long running callback executed");
  s_app_data->fired_time[0] = pbl_uptime_ticks();

  sys_psleep(250);
  return;
}

// =================================================================================
static void app_timer_callback(void *data) {
  PBL_LOG_DBG("STT app timer callback executed");
  s_app_data->fired_time[0] = pbl_uptime_ticks();
}

// =================================================================================
// Try and reschedule a wall clock work item from its function
static void reg_timer_1_callback(struct pbl_work *work) {
  PBL_LOG_DBG("STT running reg_timer_1_callback");
  if (pbl_wallclock_work_is_running(&s_app_data->reg_timers[0])) {
    PBL_LOG_DBG("STT reg_timer_1_callback rescheduling from callback for every 2 secs. ");
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_app_data->reg_timers[0], 1, 2);
  }
}

// =================================================================================
// Try and stop a wall clock work item from its function
static void reg_timer_2_callback(struct pbl_work *work) {
  PBL_LOG_DBG("STT running reg_timer_2_callback");
  if (pbl_wallclock_work_is_running(&s_app_data->reg_timers[0])) {
    PBL_LOG_DBG("STT reg_timer_2_callback deleting from callback");
    pbl_wallclock_work_stop(&s_app_data->reg_timers[0]);
  }
}

// =================================================================================
// Try and stop, then start a wall clock work item from its function
static int s_reg_timer_3_callback_count = 0;
static void reg_timer_3_callback(struct pbl_work *work) {
  s_reg_timer_3_callback_count++;
  PBL_LOG_DBG("STT running reg_timer_3_callback");
  if (pbl_wallclock_work_is_running(&s_app_data->reg_timers[0])) {
    PBL_LOG_DBG("STT reg_timer_3_callback deleting then adding from callback");
    pbl_wallclock_work_stop(&s_app_data->reg_timers[0]);
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_app_data->reg_timers[0], 1, 1);
  }
}

// =================================================================================
static void prv_cancel_timers(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_timers); i++) {
    pbl_delayed_work_cancel_sync(&s_timers[i]);
  }
  pbl_delayed_work_cancel_sync(&s_long_timer);
  pbl_delayed_work_cancel_sync(&s_repeating_timer);
}

// =================================================================================
static void menu_callback_prefix(int index, void *ctx) {
  PBL_LOG_DBG("Hit menu item %d", index);

  // Mark the layer to be updated
  layer_mark_dirty(simple_menu_layer_get_layer(s_app_data->menu_layer));

  // Cancel old timers if present
  prv_cancel_timers();
  for (int i = 0; i < NUM_MAX_TIMERS; i++) {
    s_app_data->fired_time[i] = 0;
  }

  if (s_app_data->app_timer != nullptr) {
    app_timer_cancel(s_app_data->app_timer);
    s_app_data->app_timer = 0;
  }

  // Stop old wall clock work items if present
  for (int i = 0; i < NUM_MAX_TIMERS; i++) {
    if (pbl_wallclock_work_stop(&s_app_data->reg_timers[i])) {
      PBL_LOG_DBG("STT stopped previous wall clock work %d", i);
    }
  }
}

// =================================================================================
void single_shot_timer_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  // Single shot timer
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_timers[0], PBL_MSEC(100));

  // Make sure it's marked as scheduled
  PBL_ASSERTN(pbl_delayed_work_is_pending(&s_timers[0]));
  uint32_t expire_ms = pbl_ticks_to_ms(pbl_delayed_work_remaining(&s_timers[0]));
  PBL_ASSERTN(expire_ms <= 100);
  PBL_LOG_DBG("STT firing in %d ms", (int)expire_ms);

  // Wait for it to fire
  sys_psleep(300);
  PBL_ASSERTN(s_app_data->fired_time[0] != 0);
  PBL_ASSERTN(!pbl_delayed_work_is_pending(&s_timers[0]));
}

// =================================================================================
void repeating_timer_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  // Repeating timer
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_repeating_timer,
                              PBL_MSEC(REPEATING_TIMER_PERIOD_MS));

  PBL_ASSERTN(pbl_delayed_work_is_pending(&s_repeating_timer));
  uint32_t expire_ms = pbl_ticks_to_ms(pbl_delayed_work_remaining(&s_repeating_timer));
  PBL_ASSERTN(expire_ms <= REPEATING_TIMER_PERIOD_MS);
  PBL_LOG_DBG("STT firing in %d ms", (int)expire_ms);
}

// =================================================================================
void two_timers_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  // Multiple timers
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_timers[0], PBL_MSEC(300));
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_timers[1], PBL_MSEC(100));

  // Wait for them to fire
  sys_psleep(500);
  PBL_ASSERTN(s_app_data->fired_time[0] != 0);
  PBL_ASSERTN(s_app_data->fired_time[1] != 0);
  PBL_ASSERTN(s_app_data->fired_time[1] < s_app_data->fired_time[0]);
  PBL_ASSERTN(!pbl_delayed_work_is_pending(&s_timers[0]));
  PBL_ASSERTN(!pbl_delayed_work_is_pending(&s_timers[1]));
}

// =================================================================================
void cancel_while_running_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_long_timer, PBL_MSEC(1));
  sys_psleep(50);

  // A plain cancel finds nothing pending and leaves the callback running
  PBL_ASSERTN(pbl_work_is_running(&s_long_timer.work));
  PBL_ASSERTN(!pbl_delayed_work_cancel(&s_long_timer));
  PBL_ASSERTN(pbl_work_is_running(&s_long_timer.work));

  // A sync cancel waits for it
  pbl_delayed_work_cancel_sync(&s_long_timer);
  PBL_ASSERTN(!pbl_work_is_running(&s_long_timer.work));
}

// =================================================================================
void fail_if_scheduled_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  // fail if scheduled
  PBL_ASSERTN(pbl_delayed_work_schedule(&pbl_sys_workq_high, &s_timers[0], PBL_MSEC(100)));

  // try and reschedule while it's already scheduled
  PBL_ASSERTN(!pbl_delayed_work_schedule(&pbl_sys_workq_high, &s_timers[0], PBL_MSEC(1)));
}

// =================================================================================
void evented_timer_menu_cb(int index, void *ctx) {
  void *cb_data = 0;

  menu_callback_prefix(index, ctx);

  // Test evented_timer
  s_app_data->app_timer = app_timer_register(100 /*ms*/, app_timer_callback, cb_data);
  PBL_ASSERTN(s_app_data->app_timer);
}

// =================================================================================
void stuck_callback_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  // stuck callback
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_stuck_timer, PBL_MSEC(100));
}

// =================================================================================
void reg_timer_schedule_1sec_from_cb_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  pbl_wallclock_work_init(&s_app_data->reg_timers[0], reg_timer_1_callback);
  pbl_wallclock_work_start(&pbl_sys_workq_high, &s_app_data->reg_timers[0], 1, 1);
}

// =================================================================================
void reg_timer_schedule_1min_from_cb_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  pbl_wallclock_work_init(&s_app_data->reg_timers[0], reg_timer_1_callback);
  pbl_wallclock_work_start(&pbl_sys_workq_high, &s_app_data->reg_timers[0], 60, 1);

  // The function moves it to every 2 seconds
}

// =================================================================================
void reg_timer_delete_from_cb_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  pbl_wallclock_work_init(&s_app_data->reg_timers[0], reg_timer_2_callback);
  pbl_wallclock_work_start(&pbl_sys_workq_high, &s_app_data->reg_timers[0], 1, 1);
}

// =================================================================================
void reg_timer_delete_then_add_from_cb_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);

  s_reg_timer_3_callback_count = 0;
  pbl_wallclock_work_init(&s_app_data->reg_timers[0], reg_timer_3_callback);
  pbl_wallclock_work_start(&pbl_sys_workq_high, &s_app_data->reg_timers[0], 1, 1);

  // Wait for timer to run at least twice
  PBL_LOG_DBG("waiting for callback to run 2 times");
  sys_psleep(2200);

  PBL_ASSERT(s_reg_timer_3_callback_count >= 2, "Callback didn't run at least twice");
}

// =================================================================================
void panic_menu_cb(int index, void *ctx) {
  menu_callback_prefix(index, ctx);
  pbl_panic("DIE!");
}

// =================================================================================
static void prv_window_load(Window *window) {
  TestTimersAppData *data = s_app_data;

  static const SimpleMenuItem menu_items[] = {
    {.title = "single-shot timer", .callback = single_shot_timer_menu_cb},
    {.title = "repeating timer", .callback = repeating_timer_menu_cb},
    {.title = "two timers", .callback = two_timers_menu_cb},
    {.title = "cancel while running", .callback = cancel_while_running_menu_cb},
    {.title = "fail if scheduled", .callback = fail_if_scheduled_menu_cb},
    {.title = "evented_timer", .callback = evented_timer_menu_cb},
    {.title = "stuck callback", .callback = stuck_callback_menu_cb},
    {.title = "RT: sch 1 sec from cb", .callback = reg_timer_schedule_1sec_from_cb_menu_cb},
    {.title = "RT: sch 1 min from cb", .callback = reg_timer_schedule_1min_from_cb_menu_cb},
    {.title = "RT: delete from cb", .callback = reg_timer_delete_from_cb_menu_cb},
    {.title = "RT: delete+add from cb", .callback = reg_timer_delete_then_add_from_cb_menu_cb},
    {.title = "panic", .callback = panic_menu_cb}
  };
  static const SimpleMenuSection sections[] = {
    {.items = menu_items, .num_items = ARRAY_LENGTH(menu_items)}
  };

  Layer *window_layer = window_get_root_layer(data->window);
  GRect bounds = window_layer->bounds;

  data->menu_layer =
      simple_menu_layer_create(bounds, data->window, sections, ARRAY_LENGTH(sections), nullptr);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->menu_layer));
}

// =================================================================================
// Deinitialize resources on window unload that were initialized on window load
static void prv_window_unload(Window *window) {
  simple_menu_layer_destroy(s_app_data->menu_layer);
}

// =================================================================================
static void handle_init(void) {
  TestTimersAppData *data = app_malloc_check(sizeof(TestTimersAppData));
  memset(data, 0, sizeof(TestTimersAppData));
  s_app_data = data;

  data->window = window_create();
  if (data->window == nullptr) {
    return;
  }
  window_init(data->window, "");
  window_set_window_handlers(data->window, &(WindowHandlers){
                                             .load = prv_window_load,
                                             .unload = prv_window_unload,
                                           });
  app_window_stack_push(data->window, true /*animated*/);
}

static void handle_deinit(void) {
  prv_cancel_timers();
  // The wall clock work items are in the app's heap.
  for (int i = 0; i < NUM_MAX_TIMERS; i++) {
    pbl_wallclock_work_stop(&s_app_data->reg_timers[i]);
    pbl_work_flush(&s_app_data->reg_timers[i].work);
  }
  // Don't bother freeing anything, the OS should be re-initing the heap.
}

// =================================================================================
static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

// =================================================================================
const PebbleProcessMd *test_sys_timer_app_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    .name = "System Timer Test"
  };
  return (const PebbleProcessMd *)&s_app_info;
}
