/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/prf_apps/recovery_first_use_app/getting_started_button_combo.h"

#include "clar.h"


// Stubs
///////////////////////////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_passert.h"

#include "fake_new_timer.h"

static bool s_mfg_mode_entered;

void mfg_enter_mfg_mode(void) {
  s_mfg_mode_entered = true;
}

void mfg_enter_mfg_mode_and_launch_app(void) {
  mfg_enter_mfg_mode();
}

static bool s_factory_reset_called;

void factory_reset(bool shutdown) {
  s_factory_reset_called = true;
}

Window* spinner_ui_window_get(void) {
  return NULL;
}

void app_window_stack_push(Window *window, bool animated) {
  return;
}

void system_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

void process_manager_send_callback_event_to_process(PebbleTask task, void (*callback)(void *data),
                                                    void *data) {
  callback(data);
}

void accessory_imaging_enable(bool enable) {
}

// Tests
///////////////////////////////////////////////////////////////////////////////

static GettingStartedButtonComboState s_state;

static bool s_select_cb_called;

static void prv_select_callback(void *data) {
  s_select_cb_called = true;
}

void test_getting_started_button_combo__initialize(void) {
  s_mfg_mode_entered = false;
  s_factory_reset_called = false;
  s_select_cb_called = false;

  getting_started_button_combo_init(&s_state, prv_select_callback);
}

void test_getting_started_button_combo__cleanup(void) {
  getting_started_button_combo_deinit(&s_state);
}


static StubTimer* prv_find_combo_timer(void) {
  return (StubTimer*) s_running_timers;
}

static void prv_press_button(ButtonId id) {
  getting_started_button_combo_button_pressed(&s_state, id);
}

static void prv_release_button(ButtonId id) {
  getting_started_button_combo_button_released(&s_state, id);
}

void test_getting_started_button_combo__simple(void) {
  cl_assert(!prv_find_combo_timer());

  prv_press_button(BUTTON_ID_UP);

  cl_assert(!prv_find_combo_timer());

  prv_press_button(BUTTON_ID_SELECT);

  // Make sure we've waited the appropriate amount of time
  StubTimer *timer = prv_find_combo_timer();
  cl_assert(timer);
  cl_assert_equal_i(stub_new_timer_timeout(timer->id), 5000);

  // Pretend 5000ms have elapsed
  stub_new_timer_fire(timer->id);

  // We now should be in mfg mode
  cl_assert(s_mfg_mode_entered);
}

void test_getting_started_button_combo__push_and_release_other_button(void) {
  // Up (nothing) -> Up+Select (mfg) -> Up+Select+Down (nothing) -> Up+Select (mfg)
  prv_press_button(BUTTON_ID_UP);

  prv_press_button(BUTTON_ID_SELECT);

  prv_press_button(BUTTON_ID_DOWN);

  // We should have cancelled the timer
  cl_assert(!prv_find_combo_timer());
  cl_assert(!s_mfg_mode_entered);

  prv_release_button(BUTTON_ID_DOWN);

  // Make sure we've waited the appropriate amount of time
  StubTimer *timer = prv_find_combo_timer();
  cl_assert(timer);
  cl_assert_equal_i(stub_new_timer_timeout(timer->id), 5000);

  // Pretend 5000ms have elapsed
  stub_new_timer_fire(timer->id);

  // We now should be in mfg mode
  cl_assert(s_mfg_mode_entered);
}

void test_getting_started_button_combo__push_combo_and_release_one(void) {
  // Up (nothing) -> Up+Select (mfg) -> Up (nothing) -> Up+Select (mfg)
  prv_press_button(BUTTON_ID_UP);

  prv_press_button(BUTTON_ID_SELECT);

  prv_release_button(BUTTON_ID_SELECT);

  // We should have cancelled the timer
  cl_assert(!prv_find_combo_timer());
  cl_assert(!s_mfg_mode_entered);

  prv_press_button(BUTTON_ID_SELECT);

  // Make sure we've waited the appropriate amount of time
  StubTimer *timer = prv_find_combo_timer();
  cl_assert(timer);
  cl_assert_equal_i(stub_new_timer_timeout(timer->id), 5000);

  // Pretend 5000ms have elapsed
  stub_new_timer_fire(timer->id);

  // We now should be in mfg mode
  cl_assert(s_mfg_mode_entered);
}

void test_getting_started_button_combo__push_complex_and_release_to_simple(void) {
  // Up (nothing) -> Up+Select (mfg mode) -> Select (show version)

  prv_press_button(BUTTON_ID_UP);

  prv_press_button(BUTTON_ID_SELECT);

  prv_release_button(BUTTON_ID_UP);

  // Now we're just holding down, which is a different combo

  StubTimer *timer = prv_find_combo_timer();
  cl_assert(timer);
  cl_assert_equal_i(stub_new_timer_timeout(timer->id), 5000);

  stub_new_timer_fire(timer->id);

  cl_assert(!s_mfg_mode_entered);
  cl_assert(s_select_cb_called);
}

void test_getting_started_button_combo__push_complex_and_release_to_simple_and_back_to_complex(void) {
  // Just up (nothing) -> Up+Select (mfg mode) -> Select (show version) -> Up+Select (mfg mode)

  prv_press_button(BUTTON_ID_UP);

  prv_press_button(BUTTON_ID_SELECT);

  prv_release_button(BUTTON_ID_UP);

  prv_press_button(BUTTON_ID_UP);

  StubTimer *timer = prv_find_combo_timer();
  cl_assert(timer);
  cl_assert_equal_i(stub_new_timer_timeout(timer->id), 5000);

  stub_new_timer_fire(timer->id);

  cl_assert(s_mfg_mode_entered);
  cl_assert(!s_factory_reset_called);
}

