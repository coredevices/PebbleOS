/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/common/event_service.h"
#include "pbl/services/common/touch/touch.h"
#include "pbl/services/common/touch/touch_event.h"

#include <stdbool.h>
#include <stdint.h>

#include "fake_events.h"

// Stubs
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

void kernel_free(void *p) {}

static EventServiceAddSubscriberCallback s_add_subscriber_cb;
static EventServiceRemoveSubscriberCallback s_remove_subscriber_cb;

void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback add_cb,
                        EventServiceRemoveSubscriberCallback remove_cb) {
  cl_assert_equal_i(type, PEBBLE_TOUCH_EVENT);
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
