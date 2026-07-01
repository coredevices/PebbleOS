/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "board/board.h"
#include "drivers/backlight.h"
#include "pbl/services/light.h"
#include "system/passert.h"

#include "fake_new_timer.h"

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_queue.h"
#include "stubs_fonts.h"
#include "stubs_events.h"
#include "stubs_print.h"
#include "stubs_passert.h"
#include "stubs_analytics.h"
#include "stubs_ambient_light.h"
#include "stubs_battery_monitor.h"
#include "stubs_event_service_client.h"
#include "stubs_low_power.h"
#include "stubs_serial.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_rtc.h"

void vTaskDelay(uint32_t ticks) {
}

// the time that the backlight remains on but there is zero user interaction
extern const uint32_t INACTIVE_LIGHT_TIMEOUT_MS;
// the time duration of the fade out
extern const uint32_t LIGHT_FADE_TIME_MS;
// number of fade-out steps
extern const uint32_t LIGHT_FADE_STEPS;



// Stubs
///////////////////////////////////////////////////////////

static TimerID s_light_timer;
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
static TimerID s_color_schedule_timer;
#endif

static uint8_t s_backlight_brightness;
static bool s_backlight_enabled = true;

BacklightBehaviour backlight_get_behaviour(void) {
  return BacklightBehaviour_On;
}

bool backlight_is_enabled(void) {
  return s_backlight_enabled;
}

bool backlight_is_ambient_sensor_enabled(void) {
  return false;
}

void backlight_set_enabled(bool enabled) {
  s_backlight_enabled = enabled;
}

void backlight_set_ambient_sensor_enabled(bool enabled) {
}

void backlight_set_brightness(uint8_t brightness) {
  s_backlight_brightness = brightness;
}

void backlight_refresh(void) {
}

bool backlight_is_motion_enabled(void) {
  return false;
}

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
static bool s_backlight_day_night_color_enabled;
static uint16_t s_backlight_sunrise_minute = 6 * MINUTES_PER_HOUR;
static uint16_t s_backlight_sunset_minute = 18 * MINUTES_PER_HOUR;
static uint32_t s_backlight_default_color = BACKLIGHT_COLOR_WARM_WHITE;
static uint32_t s_backlight_night_color = BACKLIGHT_COLOR_RED;
static uint32_t s_backlight_color;

bool backlight_day_night_color_is_enabled(void) {
  return s_backlight_day_night_color_enabled;
}

void backlight_day_night_color_set_enabled(bool enabled) {
  s_backlight_day_night_color_enabled = enabled;
}

uint32_t backlight_get_default_color(void) {
  return s_backlight_default_color;
}

void backlight_set_default_color(uint32_t rgb_color) {
  s_backlight_default_color = rgb_color;
}

uint32_t backlight_get_night_color(void) {
  return s_backlight_night_color;
}

void backlight_set_night_color(uint32_t rgb_color) {
  s_backlight_night_color = rgb_color;
}

uint16_t backlight_get_sunrise_minute(void) {
  return s_backlight_sunrise_minute;
}

void backlight_set_sunrise_minute(uint16_t minute) {
  s_backlight_sunrise_minute = minute;
}

uint16_t backlight_get_sunset_minute(void) {
  return s_backlight_sunset_minute;
}

void backlight_set_sunset_minute(uint16_t minute) {
  s_backlight_sunset_minute = minute;
}

void backlight_set_color(uint32_t rgb_color) {
  s_backlight_color = rgb_color;
}

uint32_t backlight_get_color(void) {
  return s_backlight_color;
}
#endif

// From pref.h
uint32_t s_backlight_timeout_ms;
uint32_t backlight_get_timeout_ms(void) {
  return s_backlight_timeout_ms;
}
void backlight_set_timeout_ms(uint32_t timeout_ms) {
  PBL_ASSERTN(timeout_ms > 0);
  s_backlight_timeout_ms = timeout_ms;
}

uint16_t s_backlight_intensity;

uint8_t backlight_get_intensity(void) {
  return s_backlight_intensity;
}

void backlight_set_intensity(uint8_t percent_intensity) {
  PBL_ASSERTN(percent_intensity > 0 && percent_intensity <= 100);
  s_backlight_intensity = percent_intensity;
}


// Helper functions
///////////////////////////////////////////////////////////

static uint8_t get_expected_brightness() {
  return backlight_get_intensity();
}

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
static void prv_set_light_timer_ids(void) {
  TimerID first_timer = TIMER_INVALID_ID;
  TimerID second_timer = TIMER_INVALID_ID;

  for (ListNode *node = s_idle_timers; node; node = list_get_next(node)) {
    StubTimer *timer = (StubTimer *)node;
    if (first_timer == TIMER_INVALID_ID) {
      first_timer = timer->id;
    } else if (second_timer == TIMER_INVALID_ID) {
      second_timer = timer->id;
    } else {
      break;
    }
  }

  cl_assert(first_timer != TIMER_INVALID_ID);
  cl_assert(second_timer != TIMER_INVALID_ID);

  if (first_timer > second_timer) {
    s_light_timer = first_timer;
    s_color_schedule_timer = second_timer;
  } else {
    s_light_timer = second_timer;
    s_color_schedule_timer = first_timer;
  }
}
#endif

static void check_on(void) {
  cl_assert_equal_i(s_backlight_brightness, get_expected_brightness());
  cl_assert(!stub_new_timer_is_scheduled(s_light_timer));
}

static void check_on_timed(void) {
  cl_assert_equal_i(s_backlight_brightness, get_expected_brightness());
  cl_assert(stub_new_timer_is_scheduled(s_light_timer));
}

// Go from timed to part way through fading
static void check_on_timed_and_consume_partial(void) {
  check_on_timed();

  stub_new_timer_fire(s_light_timer);

  cl_assert_equal_i(s_backlight_brightness, 100 - (100 / LIGHT_FADE_STEPS));
  cl_assert(stub_new_timer_is_scheduled(s_light_timer));
}

static void check_on_timed_and_consume(void) {
  check_on_timed_and_consume_partial();

  // Fire the time repeatedly to take us through the remaining steps.
  while (s_backlight_brightness) {
    stub_new_timer_fire(s_light_timer);
  }

  // We're at backlight off. There should be no more timers.
  cl_assert(!stub_new_timer_is_scheduled(s_light_timer));
}

static void check_off(void) {
  cl_assert_equal_i(s_backlight_brightness, 0);
  cl_assert(!stub_new_timer_is_scheduled(s_light_timer));
}


// Tests
///////////////////////////////////////////////////////////

void test_light__initialize(void) {
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  s_backlight_day_night_color_enabled = false;
  s_backlight_sunrise_minute = 6 * MINUTES_PER_HOUR;
  s_backlight_sunset_minute = 18 * MINUTES_PER_HOUR;
  s_backlight_default_color = BACKLIGHT_COLOR_WARM_WHITE;
  s_backlight_night_color = BACKLIGHT_COLOR_RED;
  s_backlight_color = 0;
#endif

  light_init();
  light_allow(true);
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  prv_set_light_timer_ids();
#else
  s_light_timer = ((StubTimer*) s_idle_timers)->id;
#endif
  backlight_set_intensity(100);
  s_backlight_enabled = true;
}

void test_light__cleanup(void) {
  s_backlight_brightness = 0;
  s_backlight_enabled = true;
  stub_new_timer_cleanup();
}

void test_light__button_press_and_release(void) {
  light_button_pressed();
  check_on();

  light_button_released();
  check_on_timed_and_consume();
}

void test_light__light_enable_interaction(void) {
  light_enable_interaction();
  check_on_timed_and_consume();
}

void test_light__light_enable(void) {
  light_enable(true);
  check_on();

  light_enable(true);
  check_on();

  light_enable(false);
  check_off();

  light_enable(true);
  check_on();
}

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
void test_light__backlight_color_prefs_loaded_schedules_day_night_timer(void) {
  cl_assert(!stub_new_timer_is_scheduled(s_color_schedule_timer));

  s_backlight_day_night_color_enabled = true;
  s_backlight_sunrise_minute = 6 * MINUTES_PER_HOUR;
  s_backlight_sunset_minute = 18 * MINUTES_PER_HOUR;

  light_handle_backlight_color_prefs_loaded();

  cl_assert(stub_new_timer_is_scheduled(s_color_schedule_timer));
  cl_assert(stub_new_timer_timeout(s_color_schedule_timer) > 0);
}

void test_light__backlight_color_prefs_loaded_schedules_wrap_daylight_timer(void) {
  cl_assert(!stub_new_timer_is_scheduled(s_color_schedule_timer));

  s_backlight_day_night_color_enabled = true;
  s_backlight_sunrise_minute = 3 * MINUTES_PER_HOUR;
  s_backlight_sunset_minute = 1 * MINUTES_PER_HOUR;

  light_handle_backlight_color_prefs_loaded();

  cl_assert(stub_new_timer_is_scheduled(s_color_schedule_timer));
  cl_assert(stub_new_timer_timeout(s_color_schedule_timer) > 0);
}
#endif

void test_light__light_enable_plus_wrist_shake(void) {
  light_enable(true);
  check_on();

  light_enable_interaction();
  check_on();

  light_enable(false);
  check_off();

  light_enable_interaction();
  check_on_timed_and_consume();
}

void test_light__light_enable_plus_button_pressed(void) {
  light_enable(true);
  check_on();

  light_button_pressed();
  check_on();

  light_button_released();
  check_on();

  light_enable(false);
  check_off();

  light_button_pressed();
  check_on();

  light_button_released();
  check_on_timed_and_consume();
}

void test_light__button_press_during_fading(void) {
  light_button_pressed();
  check_on();

  light_button_released();
  check_on_timed_and_consume_partial();

  light_button_pressed();
  check_on();

  light_button_released();
  check_on_timed_and_consume();
}

void test_light__toggle_disabled_while_button_pressed_turns_off_immediately(void) {
  light_button_pressed();
  check_on();

  light_toggle_enabled();
  cl_assert(!backlight_is_enabled());
  check_off();

  light_button_released();
  check_off();
}

void test_light__interaction_during_fading(void) {
  light_button_pressed();
  check_on();

  light_button_released();
  check_on_timed_and_consume_partial();

  light_enable_interaction();
  check_on_timed_and_consume();
}

void test_light__touch_down_and_up(void) {
  // A touch behaves like a button: on while down, timed out after liftoff.
  light_touch_down();
  check_on();

  light_touch_up();
  check_on_timed_and_consume();
}

void test_light__touch_down_is_coalesced(void) {
  // Repeated touch-downs take one reference; one touch-up fully releases it.
  light_touch_down();
  check_on();

  light_touch_down();
  check_on();

  light_touch_up();
  check_on_timed_and_consume();
}

void test_light__touch_up_without_down_is_noop(void) {
  // A stray liftoff must not underflow the refcount or disturb the off state.
  light_touch_up();
  check_off();

  light_button_pressed();
  check_on();
  light_button_released();
  check_on_timed_and_consume();
}

void test_light__touch_hold_released_on_app_teardown(void) {
  // App teardown must release the hold so the backlight times out, not stick on.
  light_touch_down();
  check_on();

  light_reset_user_controlled();

  check_on_timed_and_consume();
}
