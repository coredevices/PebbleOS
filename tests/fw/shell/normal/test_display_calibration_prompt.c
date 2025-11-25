/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "shell/normal/display_calibration_prompt.h"
#include "kernel/events.h"

// stubs
//////////////////////

#include "stubs_confirmation_dialog.h"
#include "stubs_dialog.h"
#include "stubs_i18n.h"
#include "stubs_logging.h"
#include "stubs_modal_manager.h"
#include "stubs_passert.h"

void window_single_click_subscribe(ButtonId button_id, ClickHandler handler) {}
void settings_display_calibration_push(WindowStack *window_stack) {}

// fakes
//////////////////////

#include "fake_new_timer.h"

static const char s_mfg_serial_failing[] = "Q402445E027E";
static const char s_mfg_serial_passing[] = "Q402445FAYYY";

static bool s_should_prompt_display_calibration;
static GPoint s_mfg_offset;
static GPoint s_user_offset;
static bool s_launcher_callback_added;
static const char *s_mfg_serial;

GPoint mfg_info_get_disp_offsets(void) {
  return s_mfg_offset;
}
const char* mfg_get_serial_number(void) {
  return s_mfg_serial;
}
GPoint shell_prefs_get_display_offset(void) {
  return gpoint_add(s_mfg_offset, s_user_offset);
}
bool shell_prefs_should_prompt_display_calibration(void) {
  return s_should_prompt_display_calibration;
}
void shell_prefs_set_should_prompt_display_calibration(bool should_prompt) {
  s_should_prompt_display_calibration = should_prompt;
}
bool gpoint_equal(const GPoint * const point_a, const GPoint * const point_b) {
  return (point_a->x == point_b->x && point_a->y == point_b->y);
}
void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  cl_assert(s_launcher_callback_added == false);
  s_launcher_callback_added = true;
}


// helpers
//////////////////////

static bool prv_does_open_dialog() {
  TimerID timer = stub_new_timer_get_next();
  if (timer == TIMER_INVALID_ID) {
    return false;
  }
  stub_new_timer_fire(stub_new_timer_get_next());
  bool callback_fired = s_launcher_callback_added;
  s_launcher_callback_added = false;
  return callback_fired;
}

// defined in display_calibration_prompt.c
bool prv_is_known_misaligned_serial_number(const char *serial);

// Tests
//////////////////////

void test_display_calibration_prompt__initialize(void) {
  s_should_prompt_display_calibration = true;
  s_mfg_offset = GPointZero;
  s_user_offset = GPointZero;
  s_launcher_callback_added = false;
  s_mfg_serial = s_mfg_serial_failing;
}

void test_display_calibration_prompt__clean_system(void) {
  // clean system startup -> open dialog
  display_calibration_prompt_show_if_needed();
  cl_assert(prv_does_open_dialog());
}

void test_display_calibration_prompt__mfg_offset(void) {
  // startup with existing mfg offset but no user offset -> open dialog
  s_mfg_offset = GPoint(0, 0);
  display_calibration_prompt_show_if_needed();
  cl_assert(prv_does_open_dialog());
}

void test_display_calibration_prompt__user_offset(void) {
  // startup with existing user offset -> don't open dialog
  s_user_offset = GPoint(1, 2);
  display_calibration_prompt_show_if_needed();
  cl_assert(!prv_does_open_dialog());
}

void test_display_calibration_prompt__prefs(void) {
  // startup with should_prompt_display_calibration already false -> don't open dialog
  s_should_prompt_display_calibration = false;
  display_calibration_prompt_show_if_needed();
  cl_assert(!prv_does_open_dialog());
}

void test_display_calibration_prompt__conditions(void) {
  // watch isn't recognized as a watch with known calibration issues -> don't open dialog
  s_mfg_serial = s_mfg_serial_passing;
  display_calibration_prompt_show_if_needed();
  cl_assert(!prv_does_open_dialog());
}

void test_display_calibration_prompt__serials(void) {
  // test the prv_is_known_misaligned_serial_number function
  cl_assert(!prv_is_known_misaligned_serial_number(s_mfg_serial_passing));
  cl_assert(prv_is_known_misaligned_serial_number(s_mfg_serial_failing));
}
