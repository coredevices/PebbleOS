/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "board/board.h"
#include "drivers/backlight.h"
#include "services/common/light.h"
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
#include "stubs_analytics_external.h"
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

static uint16_t s_backlight_brightness;

BacklightBehaviour backlight_get_behaviour(void) {
  return BacklightBehaviour_On;
}

bool backlight_is_enabled(void) {
  return true;
}

bool backlight_is_ambient_sensor_enabled(void) {
  return false;
}

void backlight_set_enabled(bool enabled) {
}

void backlight_set_ambient_sensor_enabled(bool enabled) {
}

void backlight_set_brightness(uint16_t brightness) {
  s_backlight_brightness = brightness;
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

uint16_t backlight_get_intensity(void) {
  return s_backlight_intensity;
}

uint8_t backlight_get_intensity_percent(void) {
  return (backlight_get_intensity() * 100) / BACKLIGHT_BRIGHTNESS_MAX;
}

void backlight_set_intensity_percent(uint8_t percent_intensity) {
  PBL_ASSERTN(percent_intensity > 0 && percent_intensity <= 100);
  s_backlight_intensity = (BACKLIGHT_BRIGHTNESS_MAX * (uint32_t)percent_intensity) / 100;
}


// Helper functions
///////////////////////////////////////////////////////////

static uint16_t get_expected_brightness() {
  return ((BACKLIGHT_BRIGHTNESS_MAX * backlight_get_intensity_percent()) / 100);
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

  cl_assert_equal_i(s_backlight_brightness, BACKLIGHT_BRIGHTNESS_MAX - (BACKLIGHT_BRIGHTNESS_MAX / LIGHT_FADE_STEPS));
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
  cl_assert_equal_i(s_backlight_brightness, BACKLIGHT_BRIGHTNESS_OFF);
  cl_assert(!stub_new_timer_is_scheduled(s_light_timer));
}


// Tests
///////////////////////////////////////////////////////////

void test_light__initialize(void) {
  light_init();
  light_allow(true);
  s_light_timer = ((StubTimer*) s_idle_timers)->id;
  backlight_set_intensity_percent(BOARD_CONFIG.backlight_on_percent);
}

void test_light__cleanup(void) {
  s_backlight_brightness = 0;
  stub_new_timer_delete(s_light_timer);
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

void test_light__interaction_during_fading(void) {
  light_button_pressed();
  check_on();

  light_button_released();
  check_on_timed_and_consume_partial();

  light_enable_interaction();
  check_on_timed_and_consume();
}

