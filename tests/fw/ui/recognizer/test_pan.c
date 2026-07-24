/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/recognizer/recognizer.h"
#include "applib/ui/recognizer/recognizer_impl.h"
#include "applib/ui/recognizer/recognizer_private.h"
#include "applib/ui/recognizer/pan.h"

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
void test_pan__initialize(void) {
  s_last_event = -1;
  fake_rtc_init(0, 0);
}

void test_pan__cleanup(void) {}

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

// tests

// Started fires exactly when the locked-axis movement crosses the threshold, not before. Movement
// right at the threshold (10px) is not enough; one pixel more starts the pan.
void test_pan__starts_when_threshold_crossed(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);

  // 10px in x is within the inclusive threshold: still Possible.
  prv_dispatch(r, TouchEvent_PositionUpdate, 60, 50);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Possible);

  // 11px in x crosses the threshold: Started.
  prv_dispatch(r, TouchEvent_PositionUpdate, 61, 50);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Started);
  cl_assert_equal_i(s_last_event, RecognizerEvent_Started);
}

// Movement exactly at the axis-dominance ratio (dx == 2 * dy) is ambiguous, so the pan stays
// Possible even though the threshold is crossed; one more pixel on the dominant axis makes it
// unambiguous and starts. Pins the dominance boundary that the 15/15 and 40/5 cases leave far off.
void test_pan__dominance_at_ratio_stays_possible(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  // dx = 20, dy = 10: dx == 2 * dy exactly, so neither axis dominates (threshold is crossed).
  prv_dispatch(r, TouchEvent_PositionUpdate, 70, 60);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Possible);
}

void test_pan__dominance_over_ratio_starts(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  // dx = 21, dy = 10: dx > 2 * dy, horizontal dominates and matches the lock: Started.
  prv_dispatch(r, TouchEvent_PositionUpdate, 71, 60);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Started);
}

// At the instant Started fires, delta_since_start is exactly (0, 0) (the anti-jump guarantee), while
// total_delta reflects the full movement from touchdown.
void test_pan__delta_since_start_zero_at_start(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  prv_dispatch(r, TouchEvent_PositionUpdate, 65, 50);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Started);
  cl_assert_equal_i(pan_recognizer_get_delta_since_start(r).x, 0);
  cl_assert_equal_i(pan_recognizer_get_delta_since_start(r).y, 0);
  // total_delta still measures from the touchdown point.
  cl_assert_equal_i(pan_recognizer_get_total_delta(r).x, 15);
  cl_assert_equal_i(pan_recognizer_get_total_delta(r).y, 0);
}

// After Started, each position update emits an Updated event with a growing delta_since_start; the
// per-event delta and the total delta track the movement too.
void test_pan__updated_events_grow(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  prv_dispatch(r, TouchEvent_PositionUpdate, 61, 50);  // Started, anchor at x=61
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Started);

  prv_dispatch(r, TouchEvent_PositionUpdate, 71, 50);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Updated);
  cl_assert_equal_i(s_last_event, RecognizerEvent_Updated);
  cl_assert_equal_i(pan_recognizer_get_delta_since_start(r).x, 10);
  cl_assert_equal_i(pan_recognizer_get_delta_since_prev(r).x, 10);
  cl_assert_equal_i(pan_recognizer_get_total_delta(r).x, 21);

  prv_dispatch(r, TouchEvent_PositionUpdate, 91, 50);
  cl_assert_equal_i(pan_recognizer_get_delta_since_start(r).x, 30);
  cl_assert_equal_i(pan_recognizer_get_delta_since_prev(r).x, 20);
  cl_assert_equal_i(pan_recognizer_get_total_delta(r).x, 41);
}

// A pan locked to the horizontal axis fails when the finger instead moves unambiguously along the
// vertical (foreign) axis past the threshold.
void test_pan__foreign_axis_fails(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  prv_dispatch(r, TouchEvent_PositionUpdate, 50, 61);  // 11px vertical dominates, threshold crossed

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
}

// Ambiguous (diagonal) movement, even past the threshold, keeps the recognizer Possible: it waits
// for the movement to resolve to a dominant axis.
void test_pan__ambiguous_stays_possible(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  // 15px in each axis: neither axis dominates by the 2x factor.
  prv_dispatch(r, TouchEvent_PositionUpdate, 65, 65);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Possible);

  // Resolving to a clear horizontal movement then starts the pan.
  prv_dispatch(r, TouchEvent_PositionUpdate, 90, 55);
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Started);
}

// A vertical pan starts on downward movement and completes on liftoff, reporting a non-zero
// velocity along the locked axis.
void test_pan__completes_with_velocity(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Vertical);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  prv_advance_ms(20);
  prv_dispatch(r, TouchEvent_PositionUpdate, 50, 70);  // Started
  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Started);
  prv_advance_ms(20);
  prv_dispatch(r, TouchEvent_PositionUpdate, 50, 90);  // Updated
  prv_advance_ms(20);
  // Liftoff coordinates are ignored; the end point is the last position update.
  prv_dispatch(r, TouchEvent_Liftoff, 0, 0);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
  cl_assert_equal_i(s_last_event, RecognizerEvent_Completed);
  const GPoint v = pan_recognizer_get_velocity(r);
  cl_assert(v.y > 0);
}

// A liftoff at (0, 0) does not corrupt the reported movement: the driver's
// finger-up recovery delivers (0, 0), but the total delta must stay anchored to
// the last position update, not swing back toward the origin.
void test_pan__liftoff_origin_ignored(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  prv_advance_ms(20);
  prv_dispatch(r, TouchEvent_PositionUpdate, 90, 50);  // Started, dx = 40
  prv_advance_ms(20);
  prv_dispatch(r, TouchEvent_Liftoff, 0, 0);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
  // 90 - 50 = 40, not 0 - 50 = -50 as a liftoff-coordinate leak would report.
  cl_assert_equal_i(pan_recognizer_get_total_delta(r).x, 40);
  cl_assert_equal_i(pan_recognizer_get_total_delta(r).y, 0);
}

// A liftoff before the pan ever starts (finger never crossed the threshold) fails: it is not a pan.
void test_pan__liftoff_before_start_fails(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  prv_dispatch(r, TouchEvent_PositionUpdate, 55, 50);  // only 5px, stays Possible
  prv_advance_ms(50);
  prv_dispatch(r, TouchEvent_Liftoff, 0, 0);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Failed);
}

// dt == 0: two position updates at the same tick yield zero velocity with no divide-by-zero.
void test_pan__zero_dt_velocity_zero(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  prv_dispatch(r, TouchEvent_PositionUpdate, 70, 50);  // Started, same tick as touchdown
  prv_dispatch(r, TouchEvent_Liftoff, 0, 0);

  cl_assert_equal_i(recognizer_get_state(r), RecognizerState_Completed);
  const GPoint v = pan_recognizer_get_velocity(r);
  cl_assert_equal_i(v.x, 0);
  cl_assert_equal_i(v.y, 0);
}

// A gesture with no intermediate position events reports zero velocity with no crash.
void test_pan__no_events_velocity_zero(void) {
  NEW_RECOGNIZER(r) = pan_recognizer_create(prv_event_cb, NULL, PanAxis_Horizontal);

  prv_dispatch(r, TouchEvent_Touchdown, 50, 50);
  const GPoint v = pan_recognizer_get_velocity(r);
  cl_assert_equal_i(v.x, 0);
  cl_assert_equal_i(v.y, 0);
}
