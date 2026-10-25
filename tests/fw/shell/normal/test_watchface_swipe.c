/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/rtc.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <applib/ui/recognizer/recognizer_private.h>
#include <applib/ui/recognizer/touch_nav.h>
#include <clar.h>
#include <fake_rtc.h>
#include <shell/normal/watchface_swipe.h>

static WatchfaceSwipe s_swipe;

void recognizer_manager_handle_state_change(RecognizerManager *manager, Recognizer *changed) {
}

void test_watchface_swipe__initialize(void) {
  fake_rtc_init(0, 0);
  watchface_swipe_init(&s_swipe);
}

static SwipeDirection prv_event(TouchEventType type, int16_t x, int16_t y, bool eligible,
                                bool non_navigational) {
  const TouchEvent event = {
    .type = type,
    .x = x,
    .y = y,
    .non_navigational = non_navigational,
  };
  fake_rtc_increment_ticks(20 * RTC_TICKS_HZ / PBL_MSEC_PER_SEC);
  return watchface_swipe_handle_touch(&s_swipe, &event, eligible);
}

static void prv_down(int16_t y, bool eligible, bool non_navigational) {
  cl_assert_equal_i(prv_event(TouchEvent_Touchdown, 80, y, eligible, non_navigational),
                    SwipeDirection_None);
}

static SwipeDirection prv_finish(int16_t x, int16_t y) {
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, x, y, true, false), SwipeDirection_None);
  return prv_event(TouchEvent_Liftoff, 0, 0, true, false);
}

void test_watchface_swipe__all_directions(void) {
  const struct {
    int16_t x;
    int16_t y;
    SwipeDirection direction;
  } cases[] = {
    {80, 40, SwipeDirection_Up},
    {80, 120, SwipeDirection_Down},
    {40, 80, SwipeDirection_Left},
    {120, 80, SwipeDirection_Right},
  };
  for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
    prv_down(80, true, false);
    cl_assert_equal_i(prv_finish(cases[i].x, cases[i].y), cases[i].direction);
    cl_assert_equal_i(prv_event(TouchEvent_Liftoff, 0, 0, true, false), SwipeDirection_None);
  }
}

void test_watchface_swipe__non_navigational_touchdown_latches(void) {
  prv_down(80, true, true);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
}

void test_watchface_swipe__dead_zone(void) {
  prv_down(TOUCH_NAV_STATUS_BAR_DEAD_ZONE_PX - 1, true, false);
  cl_assert_equal_i(prv_finish(80, 80), SwipeDirection_None);
  prv_down(TOUCH_NAV_STATUS_BAR_DEAD_ZONE_PX, true, false);
  cl_assert_equal_i(prv_finish(80, 80), SwipeDirection_Down);
}

void test_watchface_swipe__ineligible_touchdown_latches(void) {
  prv_down(80, false, false);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
}

void test_watchface_swipe__ineligible_mid_gesture_latches(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 100, 80, false, false),
                    SwipeDirection_None);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
}

void test_watchface_swipe__ineligible_liftoff(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 120, 80, true, false),
                    SwipeDirection_None);
  cl_assert_equal_i(prv_event(TouchEvent_Liftoff, 0, 0, false, false), SwipeDirection_None);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
}

void test_watchface_swipe__non_navigational_mid_gesture_latches(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 100, 80, true, true), SwipeDirection_None);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
}

void test_watchface_swipe__non_navigational_liftoff(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 120, 80, true, false),
                    SwipeDirection_None);
  cl_assert_equal_i(prv_event(TouchEvent_Liftoff, 0, 0, true, true), SwipeDirection_None);
}

void test_watchface_swipe__slow_swipe_and_following_events(void) {
  prv_down(80, true, false);
  fake_rtc_increment_ticks(RTC_TICKS_HZ);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 110, 80, true, false),
                    SwipeDirection_None);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
}

void test_watchface_swipe__slow_liftoff(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 120, 80, true, false),
                    SwipeDirection_None);
  fake_rtc_increment_ticks(RTC_TICKS_HZ);
  cl_assert_equal_i(prv_event(TouchEvent_Liftoff, 0, 0, true, false), SwipeDirection_None);
}

void test_watchface_swipe__crooked_swipe_and_following_events(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 100, 100, true, false),
                    SwipeDirection_None);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
}

void test_watchface_swipe__short_swipe(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_finish(80 + SWIPE_MIN_LENGTH_PX - 1, 80), SwipeDirection_None);
}

void test_watchface_swipe__missing_touchdown(void) {
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
}

void test_watchface_swipe__cancel(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 120, 80, true, false),
                    SwipeDirection_None);
  watchface_swipe_cancel(&s_swipe);
  watchface_swipe_cancel(&s_swipe);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_None);
  prv_down(80, true, false);
  cl_assert_equal_i(prv_finish(120, 80), SwipeDirection_Right);
}

void test_watchface_swipe__missed_liftoff_recovers(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 120, 80, true, false),
                    SwipeDirection_None);
  prv_down(80, true, false);
  cl_assert_equal_i(prv_finish(40, 80), SwipeDirection_Left);
}

void test_watchface_swipe__failed_swipe_recovers(void) {
  prv_down(80, true, false);
  cl_assert_equal_i(prv_event(TouchEvent_PositionUpdate, 100, 100, true, false),
                    SwipeDirection_None);
  prv_down(80, true, false);
  cl_assert_equal_i(prv_finish(40, 80), SwipeDirection_Left);
}
