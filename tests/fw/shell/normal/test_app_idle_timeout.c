/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>
#include <kernel/event_loop.h>
#include <shell/normal/app_idle_timeout.h>
#include <shell/normal/watchface.h>
#include <shell/system_shell.h>

// Stubs
/////////////////////////////////////////////////////////////////////////
#include <fake_new_timer.h>
#include <stubs_logging.h>
#include <stubs_passert.h>

static int s_watchface_launch_count;

// When set, launcher task callbacks are held until prv_run_deferred() to model queueing
static bool s_defer_callbacks;
static CallbackEventCallback s_deferred_cb;
static void *s_deferred_data;

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  if (s_defer_callbacks) {
    s_deferred_cb = callback;
    s_deferred_data = data;
    return;
  }
  callback(data);
}

static void prv_run_deferred(void) {
  s_defer_callbacks = false;
  s_deferred_cb(s_deferred_data);
}

const CompositorTransition *shell_get_watchface_compositor_animation(
    bool watchface_is_destination) {
  return nullptr;
}

void watchface_launch_default(const CompositorTransition *animation) {
  s_watchface_launch_count++;
}

// Module state under test
/////////////////////////////////////////////////////////////////////////
extern TimerID s_timer;
extern bool s_app_paused;
extern bool s_touch_held;

static bool prv_is_scheduled(void) {
  return stub_new_timer_is_scheduled(s_timer);
}

// Tests
/////////////////////////////////////////////////////////////////////////

void test_app_idle_timeout__initialize(void) {
  s_watchface_launch_count = 0;
  s_defer_callbacks = false;
  app_idle_timeout_stop();
  s_app_paused = false;
  s_touch_held = false;
}

void test_app_idle_timeout__cleanup(void) {
  app_idle_timeout_stop();
  stub_new_timer_cleanup();
}

void test_app_idle_timeout__start_schedules(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);
  cl_assert(prv_is_scheduled());
}

void test_app_idle_timeout__uses_requested_duration(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_MENU_MS);
  cl_assert_equal_i(stub_new_timer_timeout(s_timer), APP_IDLE_TIMEOUT_MENU_MS);

  app_idle_timeout_refresh();
  cl_assert_equal_i(stub_new_timer_timeout(s_timer), APP_IDLE_TIMEOUT_MENU_MS);
}

void test_app_idle_timeout__set_duration_restarts(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_MENU_MS);
  app_idle_timeout_set_duration(APP_IDLE_TIMEOUT_BLUETOOTH_MS);
  cl_assert_equal_i(stub_new_timer_timeout(s_timer), APP_IDLE_TIMEOUT_BLUETOOTH_MS);
}

void test_app_idle_timeout__set_duration_when_stopped_is_noop(void) {
  app_idle_timeout_set_duration(APP_IDLE_TIMEOUT_BLUETOOTH_MS);
  cl_assert(s_timer == TIMER_INVALID_ID);
}

void test_app_idle_timeout__set_duration_dropped_after_app_exit(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_MENU_MS);
  s_defer_callbacks = true;
  app_idle_timeout_set_duration(APP_IDLE_TIMEOUT_BLUETOOTH_MS);

  // The app exits and the next one starts before the queued change runs
  app_idle_timeout_stop();
  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);
  prv_run_deferred();

  cl_assert_equal_i(stub_new_timer_timeout(s_timer), APP_IDLE_TIMEOUT_LAUNCHER_MS);
}

void test_app_idle_timeout__touch_hold_halts_until_liftoff(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);

  app_idle_timeout_touch_down();
  cl_assert(!prv_is_scheduled());

  app_idle_timeout_touch_up();
  cl_assert(prv_is_scheduled());
}

void test_app_idle_timeout__refresh_during_hold_stays_halted(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);
  app_idle_timeout_touch_down();

  app_idle_timeout_refresh();
  cl_assert(!prv_is_scheduled());

  app_idle_timeout_touch_up();
  cl_assert(prv_is_scheduled());
}

void test_app_idle_timeout__hold_composes_with_focus_pause(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);

  // Focus lost mid-hold: liftoff must not restart the timer while paused.
  app_idle_timeout_touch_down();
  app_idle_timeout_pause();
  app_idle_timeout_touch_up();
  cl_assert(!prv_is_scheduled());
  app_idle_timeout_resume();
  cl_assert(prv_is_scheduled());

  // Focus regained mid-hold: resume must not restart the timer while held.
  app_idle_timeout_pause();
  app_idle_timeout_touch_down();
  app_idle_timeout_resume();
  cl_assert(!prv_is_scheduled());
  app_idle_timeout_touch_up();
  cl_assert(prv_is_scheduled());
}

void test_app_idle_timeout__liftoff_without_touchdown_is_harmless(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);
  app_idle_timeout_touch_up();
  cl_assert(prv_is_scheduled());
}

void test_app_idle_timeout__start_during_hold_waits_for_liftoff(void) {
  app_idle_timeout_touch_down();
  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);
  cl_assert(!prv_is_scheduled());

  app_idle_timeout_touch_up();
  cl_assert(prv_is_scheduled());
}

void test_app_idle_timeout__expiry_launches_watchface(void) {
  app_idle_timeout_start(APP_IDLE_TIMEOUT_LAUNCHER_MS);
  cl_assert(stub_new_timer_fire(s_timer));
  cl_assert_equal_i(s_watchface_launch_count, 1);
}
