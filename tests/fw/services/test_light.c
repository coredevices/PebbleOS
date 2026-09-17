/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "board/board.h"
#include <pbl/drivers/backlight.h>
#include "pbl/services/light.h"
#include "pbl/util/math.h"
#include "pbl/util/size.h"
#include "pbl/services/cron.h"
#include "system/passert.h"
#include "util/time/time.h"

#include "fake_new_timer.h"
#include "fake_rtc.h"

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_fonts.h"
#include "stubs_events.h"
#include "stubs_print.h"
#include "stubs_passert.h"
#include "stubs_analytics.h"
#include "stubs_ambient_light.h"
#include "stubs_battery_monitor.h"
#include "stubs_low_power.h"
#include "stubs_serial.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"

// the time that the backlight remains on but there is zero user interaction
extern const uint32_t INACTIVE_LIGHT_TIMEOUT_MS;
// the time duration of the fade out
extern const uint32_t LIGHT_FADE_TIME_MS;
// number of fade-out steps
extern const uint32_t LIGHT_FADE_STEPS;

// Stubs
///////////////////////////////////////////////////////////

static TimerID s_light_timer;

static uint8_t s_backlight_brightness;
static bool s_backlight_enabled = true;
static uint32_t s_backlight_color;

void backlight_set_color(uint32_t rgb_color) {
  s_backlight_color = rgb_color;
}

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

uint8_t backlight_get_level(uint8_t brightness) {
  return brightness;
}

void backlight_refresh(void) {
}

bool backlight_is_motion_enabled(void) {
  return false;
}

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

static bool s_day_night_enabled;
static uint32_t s_day_color = BACKLIGHT_COLOR_WARM_WHITE;
static uint32_t s_night_color = BACKLIGHT_COLOR_RED;
static uint16_t s_sunrise_minute;
static uint16_t s_sunset_minute;

bool backlight_day_night_color_is_enabled(void) {
  return s_day_night_enabled;
}

uint32_t backlight_get_default_color(void) {
  return s_day_color;
}

uint32_t backlight_get_night_color(void) {
  return s_night_color;
}

uint16_t backlight_get_sunrise_minute(void) {
  return s_sunrise_minute;
}

uint16_t backlight_get_sunset_minute(void) {
  return s_sunset_minute;
}

// Helper functions
///////////////////////////////////////////////////////////

static uint8_t get_expected_brightness() {
  return DIVIDE_CEIL(backlight_get_intensity() * (uint16_t)BOARD_CONFIG.backlight_on_percent, 100U);
}

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

  const uint8_t fade_brightness = 100 - (100 / LIGHT_FADE_STEPS);
  const uint8_t scaled_fade =
      DIVIDE_CEIL(fade_brightness * (uint16_t)BOARD_CONFIG.backlight_on_percent, 100U);
  cl_assert_equal_i(s_backlight_brightness, scaled_fade);
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

static void prv_set_schedule(bool enabled, int sunrise_hour, int sunset_hour) {
  s_day_night_enabled = enabled;
  s_sunrise_minute = sunrise_hour * MINUTES_PER_HOUR;
  s_sunset_minute = sunset_hour * MINUTES_PER_HOUR;
  light_handle_color_prefs_changed();
}

static void prv_set_time_of_day(int hour, int minute) {
  fake_rtc_init(0, hour * SECONDS_PER_HOUR + minute * SECONDS_PER_MINUTE);
}

void test_light__initialize(void) {
  prv_set_time_of_day(12, 0);
  cron_service_init();
  light_init();
  light_allow(true);
  s_light_timer = ((StubTimer *)s_idle_timers)->id;
  backlight_set_intensity(100);
  s_backlight_enabled = true;
  s_backlight_color = 0;
  prv_set_schedule(false, 6, 18);
}

void test_light__cleanup(void) {
  s_backlight_brightness = 0;
  s_backlight_enabled = true;
  cron_service_deinit();
  stub_new_timer_cleanup();
}

void test_light__scales_getafix_presets_upward(void) {
  static const struct {
    uint8_t intensity;
    uint8_t scaled;
  } cases[] = {
    {0, 0}, {10, 3}, {25, 7}, {50, 13}, {100, 25},
  };

  for (size_t i = 0; i < ARRAY_LENGTH(cases); i++) {
    s_backlight_intensity = cases[i].intensity;
    light_enable(true);
    cl_assert_equal_i(s_backlight_brightness, cases[i].scaled);
    light_enable(false);
  }
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

void test_light__day_night_disabled_uses_default_color(void) {
  prv_set_time_of_day(23, 0);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_day_color);
  cl_assert_equal_i(cron_service_get_job_count(), 0);
}

void test_light__day_night_picks_color_from_schedule(void) {
  prv_set_schedule(true, 6, 18);

  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_day_color);
  light_enable(false);

  prv_set_time_of_day(23, 0);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_night_color);
  light_enable(false);

  prv_set_time_of_day(5, 59);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_night_color);
  light_enable(false);

  prv_set_time_of_day(6, 0);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_day_color);
}

void test_light__day_night_schedule_wraps_midnight(void) {
  prv_set_schedule(true, 22, 4);

  prv_set_time_of_day(2, 0);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_day_color);
  light_enable(false);

  prv_set_time_of_day(12, 0);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_night_color);
}

void test_light__day_night_equal_times_disable_schedule(void) {
  prv_set_schedule(true, 6, 6);

  prv_set_time_of_day(23, 0);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_day_color);
  cl_assert_equal_i(cron_service_get_job_count(), 0);
}

void test_light__day_night_schedules_cron_jobs(void) {
  prv_set_schedule(true, 6, 18);
  cl_assert_equal_i(cron_service_get_job_count(), 2);

  prv_set_schedule(false, 6, 18);
  cl_assert_equal_i(cron_service_get_job_count(), 0);
}

void test_light__day_night_cron_retints_lit_backlight(void) {
  prv_set_schedule(true, 6, 18);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_day_color);

  prv_set_time_of_day(18, 0);
  cron_service_wakeup();
  cl_assert_equal_i(s_backlight_color, s_night_color);
  cl_assert_equal_i(cron_service_get_job_count(), 2);

  fake_rtc_init(0, SECONDS_PER_DAY + 6 * SECONDS_PER_HOUR);
  cron_service_wakeup();
  cl_assert_equal_i(s_backlight_color, s_day_color);
  cl_assert_equal_i(cron_service_get_job_count(), 2);
}

void test_light__day_night_clock_change_retints_lit_backlight(void) {
  prv_set_schedule(true, 6, 18);
  light_enable(true);
  cl_assert_equal_i(s_backlight_color, s_day_color);

  prv_set_time_of_day(23, 0);
  light_handle_clock_change();
  cl_assert_equal_i(s_backlight_color, s_night_color);
}
