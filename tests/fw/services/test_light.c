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
// breathing cycle timing
extern const uint32_t BREATHE_FADE_TIME_MS;
extern const uint8_t BREATHE_FADE_STEPS;
extern const uint32_t BREATHE_HOLD_TIME_MS;
extern const uint32_t BREATHE_OFF_TIME_MS;



// Stubs
///////////////////////////////////////////////////////////

static TimerID s_light_timer;

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
  light_init();
  light_allow(true);
  s_light_timer = ((StubTimer*) s_idle_timers)->id;
  backlight_set_intensity(100);
  s_backlight_enabled = true;
}

void test_light__cleanup(void) {
  s_backlight_brightness = 0;
  s_backlight_enabled = true;
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

// Breathe tests
///////////////////////////////////////////////////////////

static void fire_light_timer(void) {
  stub_new_timer_fire(s_light_timer);
}

void test_light__breathe_fades_in_to_target(void) {
  backlight_set_intensity(80);
  light_start_charge_breathe();

  // Initial state: brightness starts at 0, timer scheduled for first fade-in step
  cl_assert_equal_i(s_backlight_brightness, 0);
  cl_assert(stub_new_timer_is_scheduled(s_light_timer));

  // Fire all fade-in steps; brightness should reach target
  for (int i = 0; i < BREATHE_FADE_STEPS; i++) {
    fire_light_timer();
  }
  // After fade-in completes, transitions to HOLD state at target intensity
  cl_assert_equal_i(s_backlight_brightness, 80);
  cl_assert(stub_new_timer_is_scheduled(s_light_timer));
}

void test_light__breathe_hold_then_fade_out(void) {
  backlight_set_intensity(100);
  light_start_charge_breathe();

  // Advance through fade-in
  for (int i = 0; i < BREATHE_FADE_STEPS; i++) {
    fire_light_timer();
  }
  // Now in HOLD state at full brightness
  cl_assert_equal_i(s_backlight_brightness, 100);

  // Fire hold timer — transitions to FADE_OUT, first step brightness drops
  fire_light_timer();
  // Step 0 of fade-out: (STEPS - 0) / STEPS * target = 100, so brightness
  // is still 100 on the first callback call (step 0 is incremented to 1 before calc)
  // Actually step increments first then checks >= STEPS. Let's fire all fade-out steps.
  for (int i = 0; i < BREATHE_FADE_STEPS; i++) {
    fire_light_timer();
  }

  // After fade-out completes, transitions to BREATHE_OFF state at brightness 0
  cl_assert_equal_i(s_backlight_brightness, 0);
  cl_assert(stub_new_timer_is_scheduled(s_light_timer));
}

void test_light__breathe_cycle_repeats(void) {
  backlight_set_intensity(60);
  light_start_charge_breathe();

  // Fade in
  for (int i = 0; i < BREATHE_FADE_STEPS; i++) {
    fire_light_timer();
  }
  // Hold
  fire_light_timer();
  // Fade out
  for (int i = 0; i < BREATHE_FADE_STEPS; i++) {
    fire_light_timer();
  }
  // Off period — fires timer to start next fade-in
  cl_assert_equal_i(s_backlight_brightness, 0);
  fire_light_timer();

  // Should be back in FADE_IN, brightness starts ramping from 0
  cl_assert(stub_new_timer_is_scheduled(s_light_timer));
}

void test_light__breathe_stop_mid_fade(void) {
  backlight_set_intensity(100);
  light_start_charge_breathe();

  // Fire a few fade-in steps so brightness is somewhere in the middle
  fire_light_timer();

  // Stop mid-fade
  light_stop_charge_breathe();

  // Backlight should be off, no timer scheduled
  cl_assert_equal_i(s_backlight_brightness, 0);
  cl_assert(!stub_new_timer_is_scheduled(s_light_timer));
}

void test_light__breathe_stop_when_not_breathing(void) {
  // Calling stop when not in a breathe cycle should be a no-op
  light_button_pressed();
  check_on();

  light_stop_charge_breathe();

  // Should still be on — stop had no effect
  cl_assert_equal_i(s_backlight_brightness, get_expected_brightness());
}

void test_light__breathe_stop_during_hold(void) {
  backlight_set_intensity(100);
  light_start_charge_breathe();

  // Advance to hold state
  for (int i = 0; i < BREATHE_FADE_STEPS; i++) {
    fire_light_timer();
  }
  cl_assert_equal_i(s_backlight_brightness, 100);

  light_stop_charge_breathe();
  cl_assert_equal_i(s_backlight_brightness, 0);
  cl_assert(!stub_new_timer_is_scheduled(s_light_timer));
}

void test_light__breathe_stop_during_off_period(void) {
  backlight_set_intensity(100);
  light_start_charge_breathe();

  // Advance through fade-in, hold, fade-out to reach off period
  for (int i = 0; i < BREATHE_FADE_STEPS; i++) {
    fire_light_timer();
  }
  fire_light_timer(); // hold -> fade_out
  for (int i = 0; i < BREATHE_FADE_STEPS; i++) {
    fire_light_timer();
  }
  // Now in BREATHE_OFF
  cl_assert_equal_i(s_backlight_brightness, 0);

  light_stop_charge_breathe();
  cl_assert_equal_i(s_backlight_brightness, 0);
  cl_assert(!stub_new_timer_is_scheduled(s_light_timer));
}
