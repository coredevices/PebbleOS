/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/recognizer/recognizer.h"
#include "applib/ui/recognizer/recognizer_impl.h"
#include "applib/ui/recognizer/recognizer_private.h"
#include "applib/ui/recognizer/tap.h"

#include "pbl/drivers/rtc.h"

// Fakes
#include "fake_rtc.h"

// Stubs
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_logging.h"

#include "test_recognizer_impl.h"

// The manager is not under test here; swallow the notification.
void recognizer_manager_handle_state_change(RecognizerManager *manager, Recognizer *changed) {}

static RecognizerEvent s_last_event;

static void prv_event_cb(const Recognizer *recognizer, RecognizerEvent event) {
  s_last_event = event;
}

// setup and teardown
void test_tap__initialize(void) {
  s_last_event = -1;
  fake_rtc_init(0, 0);
}

void test_tap__cleanup(void) {}

// Helpers
static void prv_dispatch(Recognizer *r, TouchEventType type, int16_t x, int16_t y) {
  const TouchEvent e = {
    .type = type,
    .x = x,
    .y = y,
  };
  recognizer_handle_touch_event(r, &e);
}

static void prv_advance_ms(uint32_t ms) {
  fake_rtc_increment_ticks((RtcTicks)ms * RTC_TICKS_HZ / MS_PER_SECOND);
}

// TAP_MAX_DURATION_MS (300) is private to tap.c; the cap is part of the tap
// contract this test pins. tap.c floors ticks -> ms, so to land the floored
// duration exactly on a target ms we round the tick count UP (ceil). This keeps
// the boundary exact regardless of RTC_TICKS_HZ (1000 or 1024 by board).
#define TAP_CAP_MS (300)
static RtcTicks prv_ticks_for_floored_ms(uint32_t ms) {
  return ((RtcTicks)ms * RTC_TICKS_HZ + (MS_PER_SECOND - 1)) / MS_PER_SECOND;
}

// tests

// A short stationary press completes and reports the touched coordinate.
void test_tap__short_press_completes(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  prv_dispatch(r, TouchEvent_PositionUpdate, 51, 61);
  prv_advance_ms(100);
  prv_dispatch(r, TouchEvent_Liftoff, 51, 61);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
  cl_assert_equal_i(s_last_event, RecognizerEvent_Completed);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).x, 51);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).y, 61);
}

// A press with no intervening position update still completes on a quick liftoff.
void test_tap__no_position_update_completes(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 30, 40);
  prv_advance_ms(50);
  prv_dispatch(r, TouchEvent_Liftoff, 30, 40);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
  // With no position update, the touchdown point is the reported coordinate.
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).x, 30);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).y, 40);
}

// Movement beyond the threshold fails the recognizer (a drag, not a tap).
void test_tap__movement_beyond_threshold_fails(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  // 11px in x exceeds the 10px threshold.
  prv_dispatch(r, TouchEvent_PositionUpdate, 61, 60);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
}

// Movement right at the threshold keeps the gesture alive and completes.
void test_tap__movement_at_threshold_completes(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  // 10px in x and 10px in y are within the inclusive threshold.
  prv_dispatch(r, TouchEvent_PositionUpdate, 60, 70);
  prv_advance_ms(100);
  prv_dispatch(r, TouchEvent_Liftoff, 60, 70);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
}

// A hold longer than TAP_MAX_DURATION_MS fails on liftoff.
void test_tap__long_hold_fails(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  prv_dispatch(r, TouchEvent_PositionUpdate, 50, 60);
  prv_advance_ms(500);
  prv_dispatch(r, TouchEvent_Liftoff, 50, 60);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
}

// A hold whose floored duration is exactly the cap is still a tap (the bound is
// inclusive). Pins the inclusive edge: flipping `<=` to `<` reddens this.
void test_tap__duration_at_boundary_completes(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  fake_rtc_increment_ticks(prv_ticks_for_floored_ms(TAP_CAP_MS));
  prv_dispatch(r, TouchEvent_Liftoff, 50, 60);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
}

// One millisecond of floored duration past the cap fails. Pins the other side:
// a 100ms/500ms pair leaves the off-by-one on the calibration constant open.
void test_tap__duration_over_boundary_fails(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  fake_rtc_increment_ticks(prv_ticks_for_floored_ms(TAP_CAP_MS + 1));
  prv_dispatch(r, TouchEvent_Liftoff, 50, 60);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
}

// A liftoff at (0, 0) does not corrupt the tap coordinate: it is taken from the
// last position update, not the liftoff.
void test_tap__liftoff_coordinate_ignored(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  prv_dispatch(r, TouchEvent_PositionUpdate, 52, 63);
  prv_advance_ms(100);
  prv_dispatch(r, TouchEvent_Liftoff, 0, 0);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).x, 52);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).y, 63);
}

// After a reset the recognizer re-arms and a second tap completes again.
void test_tap__reset_and_refire(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  prv_advance_ms(100);
  prv_dispatch(r, TouchEvent_Liftoff, 50, 60);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);

  recognizer_reset(r);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Possible);

  s_last_event = -1;
  prv_dispatch(r, TouchEvent_Touchdown, 10, 20);
  prv_dispatch(r, TouchEvent_PositionUpdate, 11, 21);
  prv_advance_ms(80);
  prv_dispatch(r, TouchEvent_Liftoff, 11, 21);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
  cl_assert_equal_i(s_last_event, RecognizerEvent_Completed);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).x, 11);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).y, 21);
}

// After a failed drag, a reset re-arms the recognizer to Possible so a fresh
// short press completes again with the new coordinate (the reset counterpart of
// reset_and_refire, which re-arms from a completed tap rather than a failure).
void test_tap__reset_rearms_after_failed_drag(void) {
  NEW_RECOGNIZER(r) = tap_recognizer_create(prv_event_cb, NULL);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 60);
  prv_dispatch(r, TouchEvent_PositionUpdate, 90, 60);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);

  recognizer_reset(r);

  prv_dispatch(r, TouchEvent_Touchdown, 5, 6);
  prv_advance_ms(100);
  prv_dispatch(r, TouchEvent_Liftoff, 5, 6);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).x, 5);
  cl_assert_equal_i(tap_recognizer_get_tap_point(r).y, 6);
}
