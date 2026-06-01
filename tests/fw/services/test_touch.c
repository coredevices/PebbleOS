/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/event_service.h"
#include "pbl/services/touch/gesture_event.h"
#include "pbl/services/touch/touch.h"
#include "pbl/services/touch/touch_event.h"

#include <stdbool.h>
#include <stdint.h>

#include "fake_events.h"

// Stubs
#include "stubs_analytics.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

void kernel_free(void *p) {}

static EventServiceAddSubscriberCallback s_add_subscriber_cb;
static EventServiceRemoveSubscriberCallback s_remove_subscriber_cb;

void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback add_cb,
                        EventServiceRemoveSubscriberCallback remove_cb) {
  cl_assert(type == PEBBLE_TOUCH_EVENT || type == PEBBLE_GESTURE_EVENT);
  s_add_subscriber_cb = add_cb;
  s_remove_subscriber_cb = remove_cb;
}

static int s_touch_sensor_enable_count;
static int s_touch_sensor_disable_count;
static bool s_touch_sensor_enabled;

void touch_sensor_set_enabled(bool enabled) {
  if (enabled) {
    s_touch_sensor_enable_count++;
  } else {
    s_touch_sensor_disable_count++;
  }
  s_touch_sensor_enabled = enabled;
}

// setup and teardown
void test_touch__initialize(void) {
  fake_event_init();
  s_add_subscriber_cb = NULL;
  s_remove_subscriber_cb = NULL;
  s_touch_sensor_enable_count = 0;
  s_touch_sensor_disable_count = 0;
  s_touch_sensor_enabled = false;
  touch_init();
  touch_reset();
  // Make sure the global kill switch is reset between tests — it's a module
  // static in touch.c and a failed test could otherwise leak its state.
  touch_service_set_globally_enabled(true);
}

void test_touch__cleanup(void) {
}

static void prv_assert_touch_event(TouchEventType type, int16_t x, int16_t y) {
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_TOUCH_EVENT);
  cl_assert_equal_i(event.touch.event.type, type);
  cl_assert_equal_i(event.touch.event.x, x);
  cl_assert_equal_i(event.touch.event.y, y);
}

static void prv_assert_gesture_event(GestureEventType type, int16_t x, int16_t y) {
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(event.type, PEBBLE_GESTURE_EVENT);
  cl_assert_equal_i(event.gesture.event.type, type);
  cl_assert_equal_i(event.gesture.event.x, x);
  cl_assert_equal_i(event.gesture.event.y, y);
}

// tests
void test_touch__touchdown(void) {
  touch_handle_update(TouchState_FingerDown, 15, 100);
  cl_assert_equal_i(fake_event_get_count(), 1);
  prv_assert_touch_event(TouchEvent_Touchdown, 15, 100);
}

void test_touch__liftoff(void) {
  touch_handle_update(TouchState_FingerDown, 15, 100);
  touch_handle_update(TouchState_FingerUp, 20, 120);
  cl_assert_equal_i(fake_event_get_count(), 2);
  prv_assert_touch_event(TouchEvent_Liftoff, 20, 120);
}

void test_touch__position_update(void) {
  touch_handle_update(TouchState_FingerDown, 10, 10);
  touch_handle_update(TouchState_FingerDown, 13, 13);
  cl_assert_equal_i(fake_event_get_count(), 2);
  prv_assert_touch_event(TouchEvent_PositionUpdate, 13, 13);

  touch_handle_update(TouchState_FingerDown, 18, 5);
  cl_assert_equal_i(fake_event_get_count(), 3);
  prv_assert_touch_event(TouchEvent_PositionUpdate, 18, 5);
}

void test_touch__position_stationary(void) {
  touch_handle_update(TouchState_FingerDown, 10, 10);
  fake_event_reset_count();
  // No event generated when position is unchanged
  touch_handle_update(TouchState_FingerDown, 10, 10);
  cl_assert_equal_i(fake_event_get_count(), 0);
}

void test_touch__no_event_when_idle(void) {
  // Liftoff while already up produces no event
  touch_handle_update(TouchState_FingerUp, 0, 0);
  cl_assert_equal_i(fake_event_get_count(), 0);
}

void test_touch__reset_clears_state(void) {
  touch_handle_update(TouchState_FingerDown, 10, 10);
  touch_reset();
  fake_event_reset_count();

  // After reset, a FingerDown update should emit a Touchdown (not PositionUpdate)
  touch_handle_update(TouchState_FingerDown, 50, 50);
  cl_assert_equal_i(fake_event_get_count(), 1);
  prv_assert_touch_event(TouchEvent_Touchdown, 50, 50);
}

void test_touch__subscriber_enables_sensor(void) {
  cl_assert(s_add_subscriber_cb != NULL);
  cl_assert(s_remove_subscriber_cb != NULL);

  s_add_subscriber_cb(PebbleTask_App);
  cl_assert_equal_i(s_touch_sensor_enable_count, 1);
  cl_assert(s_touch_sensor_enabled);

  // Additional subscribers don't re-enable the sensor
  s_add_subscriber_cb(PebbleTask_App);
  cl_assert_equal_i(s_touch_sensor_enable_count, 1);

  // Sensor stays enabled until the last subscriber leaves
  s_remove_subscriber_cb(PebbleTask_App);
  cl_assert_equal_i(s_touch_sensor_disable_count, 0);
  cl_assert(s_touch_sensor_enabled);

  s_remove_subscriber_cb(PebbleTask_App);
  cl_assert_equal_i(s_touch_sensor_disable_count, 1);
  cl_assert(!s_touch_sensor_enabled);
}

void test_touch__backlight_toggles_sensor(void) {
  touch_set_backlight_enabled(true);
  cl_assert_equal_i(s_touch_sensor_enable_count, 1);
  cl_assert(s_touch_sensor_enabled);

  // Idempotent: enabling again is a no-op
  touch_set_backlight_enabled(true);
  cl_assert_equal_i(s_touch_sensor_enable_count, 1);

  touch_set_backlight_enabled(false);
  cl_assert_equal_i(s_touch_sensor_disable_count, 1);
  cl_assert(!s_touch_sensor_enabled);

  // Idempotent: disabling again is a no-op
  touch_set_backlight_enabled(false);
  cl_assert_equal_i(s_touch_sensor_disable_count, 1);
}

void test_touch__backlight_and_app_share_sensor(void) {
  touch_set_backlight_enabled(true);
  cl_assert_equal_i(s_touch_sensor_enable_count, 1);

  // App subscription while backlight already holds the sensor: no extra enable
  s_add_subscriber_cb(PebbleTask_App);
  cl_assert_equal_i(s_touch_sensor_enable_count, 1);

  // Dropping the backlight while an app subscriber remains keeps the sensor on
  touch_set_backlight_enabled(false);
  cl_assert_equal_i(s_touch_sensor_disable_count, 0);
  cl_assert(s_touch_sensor_enabled);

  s_remove_subscriber_cb(PebbleTask_App);
  cl_assert_equal_i(s_touch_sensor_disable_count, 1);
  cl_assert(!s_touch_sensor_enabled);
}

void test_touch__has_app_subscribers_app(void) {
  cl_assert(!touch_has_app_subscribers());

  s_add_subscriber_cb(PebbleTask_App);
  cl_assert(touch_has_app_subscribers());

  s_remove_subscriber_cb(PebbleTask_App);
  cl_assert(!touch_has_app_subscribers());
}

void test_touch__has_app_subscribers_backlight(void) {
  // The backlight subscription must not register as an app subscriber: the
  // event loop uses touch_has_app_subscribers() as an override that fires the
  // backlight on any gesture, so counting the backlight's own subscription
  // would defeat the wake-gesture filter.
  cl_assert(!touch_has_app_subscribers());

  touch_set_backlight_enabled(true);
  cl_assert(!touch_has_app_subscribers());

  // With an app also subscribed, the call reflects the app, regardless of the
  // backlight subscription state.
  s_add_subscriber_cb(PebbleTask_App);
  cl_assert(touch_has_app_subscribers());

  touch_set_backlight_enabled(false);
  cl_assert(touch_has_app_subscribers());

  s_remove_subscriber_cb(PebbleTask_App);
  cl_assert(!touch_has_app_subscribers());
}

void test_touch__globally_enabled_default_true(void) {
  // Default state after init is enabled; no setter call required.
  cl_assert(touch_service_is_globally_enabled());
}

void test_touch__global_disable_drops_events(void) {
  touch_service_set_globally_enabled(false);
  touch_handle_update(TouchState_FingerDown, 10, 20);
  cl_assert_equal_i(fake_event_get_count(), 0);

  touch_handle_update(TouchState_FingerUp, 10, 20);
  cl_assert_equal_i(fake_event_get_count(), 0);

  touch_service_set_globally_enabled(true);
}

void test_touch__global_disable_powers_down_sensor(void) {
  // Subscriber brings the sensor up.
  s_add_subscriber_cb(PebbleTask_App);
  cl_assert(s_touch_sensor_enabled);
  cl_assert_equal_i(s_touch_sensor_enable_count, 1);

  // Disabling globally powers the sensor down even though a subscriber remains.
  touch_service_set_globally_enabled(false);
  cl_assert(!s_touch_sensor_enabled);
  cl_assert_equal_i(s_touch_sensor_disable_count, 1);

  // Re-enabling brings it back up for the existing subscriber.
  touch_service_set_globally_enabled(true);
  cl_assert(s_touch_sensor_enabled);
  cl_assert_equal_i(s_touch_sensor_enable_count, 2);

  s_remove_subscriber_cb(PebbleTask_App);
}

void test_touch__gesture_tap(void) {
  touch_handle_gesture(TouchGesture_Tap, 30, 40);
  cl_assert_equal_i(fake_event_get_count(), 1);
  prv_assert_gesture_event(GestureEvent_Tap, 30, 40);
}

void test_touch__gesture_double_tap(void) {
  touch_handle_gesture(TouchGesture_DoubleTap, 12, 80);
  cl_assert_equal_i(fake_event_get_count(), 1);
  prv_assert_gesture_event(GestureEvent_DoubleTap, 12, 80);
}

void test_touch__gesture_swipes(void) {
  touch_handle_gesture(TouchGesture_SwipeUp, 1, 2);
  prv_assert_gesture_event(GestureEvent_SwipeUp, 1, 2);

  touch_handle_gesture(TouchGesture_SwipeDown, 3, 4);
  prv_assert_gesture_event(GestureEvent_SwipeDown, 3, 4);

  touch_handle_gesture(TouchGesture_SwipeLeft, 5, 6);
  prv_assert_gesture_event(GestureEvent_SwipeLeft, 5, 6);

  touch_handle_gesture(TouchGesture_SwipeRight, 7, 8);
  prv_assert_gesture_event(GestureEvent_SwipeRight, 7, 8);

  cl_assert_equal_i(fake_event_get_count(), 4);
}

void test_touch__gesture_rotation_mirrors_swipes(void) {
  touch_set_rotated(true);

  // Coordinates are mirrored in the rotated frame; swipe directions flip pairwise.
  const int16_t x_rot = (DISP_COLS - 1) - 10;
  const int16_t y_rot = (DISP_ROWS - 1) - 20;

  touch_handle_gesture(TouchGesture_SwipeUp, 10, 20);
  prv_assert_gesture_event(GestureEvent_SwipeDown, x_rot, y_rot);

  touch_handle_gesture(TouchGesture_SwipeDown, 10, 20);
  prv_assert_gesture_event(GestureEvent_SwipeUp, x_rot, y_rot);

  touch_handle_gesture(TouchGesture_SwipeLeft, 10, 20);
  prv_assert_gesture_event(GestureEvent_SwipeRight, x_rot, y_rot);

  touch_handle_gesture(TouchGesture_SwipeRight, 10, 20);
  prv_assert_gesture_event(GestureEvent_SwipeLeft, x_rot, y_rot);

  touch_set_rotated(false);
}

void test_touch__gesture_rotation_taps_keep_type(void) {
  // Taps don't have a direction, so rotation only flips coordinates.
  touch_set_rotated(true);

  const int16_t x_rot = (DISP_COLS - 1) - 5;
  const int16_t y_rot = (DISP_ROWS - 1) - 7;

  touch_handle_gesture(TouchGesture_Tap, 5, 7);
  prv_assert_gesture_event(GestureEvent_Tap, x_rot, y_rot);

  touch_handle_gesture(TouchGesture_DoubleTap, 5, 7);
  prv_assert_gesture_event(GestureEvent_DoubleTap, x_rot, y_rot);

  touch_set_rotated(false);
}

void test_touch__subscribe_while_disabled_keeps_sensor_off(void) {
  touch_service_set_globally_enabled(false);

  // Subscribing while disabled must not power the sensor up.
  s_add_subscriber_cb(PebbleTask_App);
  cl_assert(!s_touch_sensor_enabled);
  cl_assert_equal_i(s_touch_sensor_enable_count, 0);

  // Re-enabling globally powers it up for the pre-existing subscriber.
  touch_service_set_globally_enabled(true);
  cl_assert(s_touch_sensor_enabled);
  cl_assert_equal_i(s_touch_sensor_enable_count, 1);

  s_remove_subscriber_cb(PebbleTask_App);
}
