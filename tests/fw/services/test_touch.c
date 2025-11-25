/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gtypes.h"
#include "kernel/events.h"
#include "services/common/touch/touch.h"
#include "services/common/touch/touch_event.h"
#include "services/common/touch/touch_client.h"
#include "util/size.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "fake_events.h"

// Stubs
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

bool gpoint_equal(const GPoint * const point_a, const GPoint * const point_b) {
  return (point_a->x == point_b->x && point_a->y == point_b->y);
}

void kernel_free(void *p) {

}

extern TouchEvent *touch_event_queue_get_event(TouchIdx touch_idx, uint32_t queue_idx);
extern void touch_set_touch_state(TouchIdx touch_idx, TouchState touch_state, GPoint touch_down_pos,
                                  uint64_t touch_down_time_ms, TouchPressure touch_down_pressure);

// setup and teardown
void test_touch__initialize(void) {
  fake_event_init();
  touch_reset();
}

void test_touch__cleanup(void) {

}

void prv_test_touch_event(TouchEvent *touch_event, TouchIdx idx, TouchEventType type, GPoint *start_pos,
                          uint64_t start_time_ms, TouchPressure start_pressure, GPoint *diff_pos,
                          uint64_t diff_time_ms, TouchPressure diff_pressure, bool test_diff) {
  cl_assert(touch_event);
  cl_assert_equal_i(touch_event->type, type);
  cl_assert_equal_i(touch_event->index, idx);
  cl_assert_equal_i(touch_event->start_time_ms, start_time_ms);
  cl_assert(gpoint_equal(&touch_event->start_pos, start_pos));
  cl_assert_equal_i(touch_event->start_pressure, start_pressure);
  if (type != TouchEvent_Touchdown) {
    cl_assert_equal_i(touch_event->diff_time_ms, diff_time_ms);
    cl_assert(gpoint_equal(&touch_event->diff_pos, diff_pos));
    cl_assert_equal_i(touch_event->diff_pressure, diff_pressure);
  }
}

// tests
void test_touch__handle_update_touchdown(void) {
  touch_handle_update(0, TouchState_FingerDown, &GPoint(15, 100), 3, 3686400);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.type, PebbleTouchEvent_TouchesAvailable);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_Touchdown, &GPoint(15, 100), 3686400, 3,
                       &GPointZero, 0, 0, true);

  touch_event = touch_event_queue_get_event(1, 0);
  cl_assert_equal_p(touch_event, NULL);

  // Test second touch
  touch_handle_update(1, TouchState_FingerDown, &GPoint(1, 13), 5, 3686401);
  event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.type, PebbleTouchEvent_TouchesAvailable);
  touch_event = touch_event_queue_get_event(1, 0);
  prv_test_touch_event(touch_event, 1, TouchEvent_Touchdown, &GPoint(1, 13), 3686401, 5,
                       &GPointZero, 0, 0, true);
}

void test_touch__handle_update_liftoff(void) {
  // Test first touch
  touch_set_touch_state(0, TouchState_FingerDown, GPointZero, 3686380, 0);
  touch_handle_update(0, TouchState_FingerUp, &GPoint(15, 100), 0, 3686400);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.type, PebbleTouchEvent_TouchesAvailable);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_Liftoff, &GPointZero, 3686380, 0,
                       &GPoint(15, 100), 20, 0, true);

  // Ensure nothing recorded for second touch
  touch_event = touch_event_queue_get_event(1, 0);
  cl_assert_equal_p(touch_event, NULL);

  // Test second touch
  touch_set_touch_state(1, TouchState_FingerDown, GPointZero, 0, 0);
  touch_handle_update(1, TouchState_FingerUp, &GPoint(1, 13), 0, 3686401);
  event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.type, PebbleTouchEvent_TouchesAvailable);
  touch_event = touch_event_queue_get_event(1, 0);
  prv_test_touch_event(touch_event, 1, TouchEvent_Liftoff, &GPointZero, 0, 0, &GPoint(1, 13),
                       3686401, 0, true);
}

void test_touch__handle_update_liftoff_null_pos(void) {
  touch_handle_update(0, TouchState_FingerDown, &GPoint(1, 13), 5, 3686400);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_Touchdown, &GPoint(1, 13), 3686400, 5,
                       &GPointZero, 0, 0, false);
  touch_handle_update(0, TouchState_FingerUp, NULL, 0, 3686410);
  touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_Liftoff, &GPoint(1, 13), 3686400, 5, &GPointZero,
                       10, -5, true);
}

void test_touch__handle_update_position(void) {
  touch_set_touch_state(0, TouchState_FingerDown, GPointZero, 3686380, 0);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(10, 10), 5, 3686400);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.type, PebbleTouchEvent_TouchesAvailable);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(10, 10), 20, 5, true);

  fake_event_reset_count();
  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  cl_assert_equal_i(fake_event_get_count(), 0);  // no event if previous one not handled

  touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(13, 13), 40, 6, true);

}

void test_touch__handle_update_position_stationary(void) {
  touch_set_touch_state(0, TouchState_FingerDown, GPointZero, 3686380, 0);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(10, 10), 5, 3686400);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(10, 10), 20, 5, true);
  // No touch event generated when finger remains stationary
  touch_handle_update(0, TouchState_FingerDown, &GPoint(10, 10), 5, 3686420);
  touch_event = touch_event_queue_get_event(0, 1);
  cl_assert_equal_p(touch_event, NULL);
}

void test_touch__handle_update_merge_position(void) {
  touch_set_touch_state(0, TouchState_FingerDown, GPointZero, 3686380, 0);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(10, 10), 5, 3686400);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(10, 10), 20, 5, true);

  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(13, 13), 40, 6, true);

  touch_handle_update(0, TouchState_FingerDown, &GPoint(18, 5), 1, 3686440);
  // Test the same event (event at index 1): it should update to reflect the difference between this
  // and the first event
  touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(18, 5), 60, 1, true);
}

void test_touch__handle_update_merge_liftoff(void) {
  touch_set_touch_state(0, TouchState_FingerDown, GPointZero, 3686380, 0);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(10, 10), 5, 3686400);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(10, 10), 20, 5, true);

  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(13, 13), 40, 6, true);

  touch_handle_update(0, TouchState_FingerUp, &GPoint(18, 5), 0, 3686440);
  // Test the same event (event at index 1): it should update to reflect the difference between this
  // and the first event and that it is a liftoff event
  touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_Liftoff, &GPointZero, 3686380, 0, &GPoint(18, 5),
                       60, 0, true);
}

void test_touch__handle_update_merge_liftoff_null_pos(void) {
  touch_set_touch_state(0, TouchState_FingerDown, GPointZero, 3686380, 0);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(10, 10), 5, 3686400);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(10, 10), 20, 5, true);

  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_PositionUpdate, &GPointZero, 3686380, 0,
                       &GPoint(13, 13), 40, 6, true);

  touch_handle_update(0, TouchState_FingerUp, NULL, 0, 3686440);
  touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_Liftoff,  &GPointZero, 3686380, 0,
                       &GPoint(13, 13), 60, 0, true);
}

void test_touch__assert_null_pos_not_liftoff(void) {
  // NULL position not valid for touchdown event
  cl_assert_passert(touch_handle_update(0, TouchState_FingerDown, NULL, 5, 3686400));

  touch_set_touch_state(0, TouchState_FingerDown, GPointZero, 0, 0);
  // NULL position not valid for position update event
  cl_assert_passert(touch_handle_update(0, TouchState_FingerDown, NULL, 5, 3686400));
}

void test_touch__handle_update_reset_queue_touchdown(void) {
  touch_set_touch_state(0, TouchState_FingerDown, GPointZero, 3686380, 0);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_handle_update(0, TouchState_FingerUp, &GPoint(15, 100), 0, 3686400);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 1);
  prv_test_touch_event(touch_event, 0, TouchEvent_Liftoff, &GPointZero, 3686380, 0,
                       &GPoint(15, 100), 20, 0, true);

  // touchdown event should reset the touch event queue regardless of what is in it
  touch_handle_update(0, TouchState_FingerDown, &GPoint(31, 1), 6, 3686500);
  touch_event = touch_event_queue_get_event(0, 0);
  prv_test_touch_event(touch_event, 0, TouchEvent_Touchdown, &GPoint(31, 1), 3686500, 6,
                       &GPointZero, 0, 0, true);
  touch_event = touch_event_queue_get_event(0, 1);
  cl_assert_equal_p(touch_event, NULL);
}

void test_touch__handle_update_pressure(void) {
  //TODO: We're not passing pressure updates to the UI yet (not so useful?)
}

typedef struct TouchEventContext {
  TouchEvent touch_events[4];
  uint32_t idx;
} TouchEventContext;

static void prv_touch_event_dispatch_cb(const TouchEvent *event, void *context) {
  TouchEventContext *ctx = context;
  cl_assert(ctx->idx < ARRAY_LENGTH(ctx->touch_events));
  ctx->touch_events[ctx->idx++] = *event;
}

void test_touch__dispatch_touch_events_single_finger(void) {
  TouchEventContext ctx = {
    .idx = 0
  };
  touch_dispatch_touch_events(0, prv_touch_event_dispatch_cb, &ctx);
  cl_assert_equal_i(ctx.idx, 0);

  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(15, 15), 6, 3686440);
  touch_dispatch_touch_events(0, prv_touch_event_dispatch_cb, &ctx);
  cl_assert_equal_i(ctx.idx, 2);
  prv_test_touch_event(&ctx.touch_events[0], 0, TouchEvent_Touchdown, &GPoint(13, 13), 3686420, 6,
                       NULL, 0, 0, false);
  prv_test_touch_event(&ctx.touch_events[1], 0, TouchEvent_PositionUpdate, &GPoint(13, 13), 3686420,
                       6, &GPoint(2, 2), 20, 0, true);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  cl_assert_equal_p(touch_event, NULL);
}

void test_touch__dispatch_touch_events_two_fingers(void) {
  TouchEventContext ctx = {
    .idx = 0
  };
  touch_dispatch_touch_events(0, prv_touch_event_dispatch_cb, &ctx);
  cl_assert_equal_i(ctx.idx, 0);

  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(15, 15), 6, 3686440);
  touch_handle_update(1, TouchState_FingerDown, &GPoint(55, 55), 2, 3686480);
  touch_handle_update(1, TouchState_FingerDown, &GPoint(33, 33), 7, 3686500);
  touch_dispatch_touch_events(0, prv_touch_event_dispatch_cb, &ctx);
  cl_assert_equal_i(ctx.idx, 2);
  prv_test_touch_event(&ctx.touch_events[0], 0, TouchEvent_Touchdown, &GPoint(13, 13), 3686420, 6,
                       NULL, 0, 0, false);
  prv_test_touch_event(&ctx.touch_events[1], 0, TouchEvent_PositionUpdate, &GPoint(13, 13), 3686420,
                       6, &GPoint(2, 2), 20, 0, true);

  touch_dispatch_touch_events(1, prv_touch_event_dispatch_cb, &ctx);
  cl_assert_equal_i(ctx.idx, 4);
  prv_test_touch_event(&ctx.touch_events[2], 1, TouchEvent_Touchdown, &GPoint(55, 55), 3686480, 2,
                       NULL, 0, 0, false);
  prv_test_touch_event(&ctx.touch_events[3], 1, TouchEvent_PositionUpdate, &GPoint(55, 55),
                       3686480, 2, &GPoint(-22, -22), 20, 5, true);
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  cl_assert_equal_p(touch_event, NULL);
}

void test_touch__cancel_touches(void) {
  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(15, 15), 6, 3686440);
  touch_handle_update(1, TouchState_FingerDown, &GPoint(55, 55), 2, 3686480);
  touch_handle_update(1, TouchState_FingerDown, &GPoint(33, 33), 7, 3686500);

  touch_handle_driver_event(TouchDriverEvent_ControllerError);
  PebbleEvent event = fake_event_get_last();
  // Touches cancelled event generated
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.type, PebbleTouchEvent_TouchesCancelled);

  // no more touches
  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  cl_assert_equal_p(touch_event, NULL);
  touch_event = touch_event_queue_get_event(1, 0);
  cl_assert_equal_p(touch_event, NULL);
}

// test that the first dispatch after a cancel event is pended does not return any touches, even
// if new touches have arrived - this is to ensure that the valid new touches are not cancelled
// by the cancellation event if it is pended before previous touches
void test_touch__cancel_touches_handle_first_dispatch(void) {
  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_handle_driver_event(TouchDriverEvent_ControllerError);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.type, PebbleTouchEvent_TouchesCancelled);
  touch_handle_update(0, TouchState_FingerDown, &GPoint(15, 15), 6, 3686440);
  // make sure that another event is, in fact, pended
  event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.type, PebbleTouchEvent_TouchesAvailable);

  TouchEventContext ctx = {
    .idx = 0
  };
  // handle first TouchesAvailable event
  touch_dispatch_touch_events(0, prv_touch_event_dispatch_cb, &ctx);
  cl_assert_equal_i(ctx.idx, 0);

  // handle second TouchesAvailable event
  touch_dispatch_touch_events(0, prv_touch_event_dispatch_cb, &ctx);
  cl_assert_equal_i(ctx.idx, 1);
  prv_test_touch_event(&ctx.touch_events[0], 0, TouchEvent_Touchdown, &GPoint(15, 15), 3686440, 6,
                       NULL, 0, 0, false);
}

static PebbleEvent s_expected_palm_events[2];
static int s_palm_event_count = 0;

static void prv_handle_palm_events(PebbleEvent *e) {
  s_expected_palm_events[s_palm_event_count++] = *e;
}

void test_touch__palm_detect_event(void) {
  touch_handle_update(0, TouchState_FingerDown, &GPoint(13, 13), 6, 3686420);
  touch_handle_update(1, TouchState_FingerDown, &GPoint(55, 55), 2, 3686480);

  fake_event_set_callback(prv_handle_palm_events);
  touch_handle_driver_event(TouchDriverEvent_PalmDetect);

  // Cancelled event, followed by a palm detection event
  cl_assert_equal_i(s_expected_palm_events[0].type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(s_expected_palm_events[0].touch.type, PebbleTouchEvent_TouchesCancelled);
  cl_assert_equal_i(s_expected_palm_events[1].type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(s_expected_palm_events[1].touch.type, PebbleTouchEvent_PalmDetected);

  TouchEvent *touch_event = touch_event_queue_get_event(0, 0);
  cl_assert_equal_p(touch_event, NULL);
  touch_event = touch_event_queue_get_event(1, 0);
  cl_assert_equal_p(touch_event, NULL);
}
