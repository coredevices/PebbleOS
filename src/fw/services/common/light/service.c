/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/common/light.h"

#include "board/board.h"
#include "drivers/ambient_light.h"
#include "drivers/backlight.h"
#include "drivers/rtc.h"
#include "kernel/low_power.h"
#include "pbl/services/common/analytics/analytics.h"
#include "pbl/services/common/battery/battery_monitor.h"
#include "pbl/services/common/new_timer/new_timer.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "os/mutex.h"
#include "system/passert.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdlib.h>

typedef enum {
  LIGHT_STATE_ON = 1,           // backlight on, no timeouts
  LIGHT_STATE_ON_TIMED = 2,     // backlight on, will start fading after a period
  LIGHT_STATE_ON_FADING = 3,    // backlight in the process of fading out
  LIGHT_STATE_OFF = 4,          // backlight off; idle state
} BacklightState;

// the time duration of the fade out
const uint32_t LIGHT_FADE_TIME_MS = 500;
// number of fade-out steps
const uint32_t LIGHT_FADE_STEPS = 20;

/*
 *              ^
 *              |
 *     LIGHT_ON |            +---------------------------------+
 *              |           /                                   \
 *              |          /                                     \
 *              |         /                                       \
 *              |        /                                         \
 *              |       /                                           \
 *  LIGHT_ON/2  |      /+                                           +\
 *              |     / |                                           | \
 *              |    /  |                                           |  \
 *              |   /   |                                           |   \
 *              |  /    |                                           |    \
 *              | /     |                                           |     \
 *              |/      |                                           |      \
 *    LIGHT_OFF +-------|-------------------------------------------|--------->
 *                      |                                           |
 *                      |<----------------------------------------->|
 *                          Integrate over this range for the mean
 */

//! The current state of the backlight (example: ON/ON_TIMED/ON_FADING).
static BacklightState s_light_state;

//! The brightness of the display in a range between BACKLIGHT_BRIGHTNESS_OFF and BACKLIGHT_BRIGHTNESS_ON
static int32_t s_current_brightness;

//! Timer to count down from the LIGHT_STATE_ON_TIMED state.
static TimerID s_timer_id;

//! Buffer for ALS samples taken right before backlight turns on
#define ALS_SAMPLE_BUFFER_SIZE 5
static uint32_t s_als_sample_buffer[ALS_SAMPLE_BUFFER_SIZE];
static uint8_t s_als_sample_count = 0;

//! Refcount of the number of buttons that are currently pushed
static int s_num_buttons_down;

//! The current app is forcing the light on and off, don't muck with it.
static bool s_user_controlled_state;

//! For temporary disabling backlight (ie: low power mode)
static bool s_backlight_allowed = false;

//! Starting intensity for fade-out (captured when fade begins)
static uint16_t s_fade_start_intensity = 0;

//! Fade step size (calculated once at start of fade to avoid rounding jitter)
static uint16_t s_fade_step_size = 0;

//! Mutex to guard all the above state. We have a pattern of taking the lock in the public functions and assuming
//! it's already taken in the prv_ functions.
static PebbleMutex *s_mutex;

//! Analytics: Track time-weighted average intensity
static uint64_t s_intensity_time_product_sum; // Sum of (intensity_pct × time_ms)
static RtcTicks s_last_intensity_sample_ticks; // Timestamp of last sample
static uint8_t s_last_sampled_intensity_pct; // Last intensity percentage sampled
static uint32_t s_total_on_time_ms; // Total backlight on time tracked internally

static void prv_change_state(BacklightState new_state);

static void light_timer_callback(void *data) {
  mutex_lock(s_mutex);
  prv_change_state(LIGHT_STATE_ON_FADING);
  mutex_unlock(s_mutex);
}

// Take multiple ALS samples right before turning on backlight (avoids backlight interference)
static void prv_sample_als_multiple_times(void) {
  s_als_sample_count = 0;

  for (uint8_t i = 0; i < ALS_SAMPLE_BUFFER_SIZE; i++) {
    s_als_sample_buffer[i] = ambient_light_get_light_level();
    s_als_sample_count++;

    // Small delay between samples (10ms) to get slightly different readings
    if (i < ALS_SAMPLE_BUFFER_SIZE - 1) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT && !defined(RECOVERY_FW)
// Check if all samples in buffer are below or equal to threshold (for Zone 1 detection)
static bool prv_all_samples_below_or_equal(uint32_t threshold) {
  if (s_als_sample_count == 0) {
    return false;  // No samples yet
  }

  for (uint8_t i = 0; i < s_als_sample_count; i++) {
    if (s_als_sample_buffer[i] > threshold) {
      return false;  // At least one sample is above threshold
    }
  }
  return true;  // All samples are at or below threshold
}

// Calculate average of samples in buffer (for Zone 2/3 decision)
static uint32_t prv_get_als_average(void) {
  if (s_als_sample_count == 0) {
    return 0;
  }

  uint32_t sum = 0;
  for (uint8_t i = 0; i < s_als_sample_count; i++) {
    sum += s_als_sample_buffer[i];
  }

  return sum / s_als_sample_count;
}
#endif // CAPABILITY_HAS_DYNAMIC_BACKLIGHT && !RECOVERY_FW

static uint16_t prv_backlight_get_intensity(void) {
  // low_power_mode backlight intensity (25% of max brightness)
  const uint16_t backlight_low_power_intensity = (BACKLIGHT_BRIGHTNESS_MAX * (uint32_t)25) / 100;
  
  if (low_power_is_active()) {
    return backlight_low_power_intensity;
  }
  
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT && !defined(RECOVERY_FW)
  // Dynamic backlight: 3-zone algorithm based on ambient light sensor
  if (backlight_is_dynamic_intensity_enabled()) {
    uint16_t user_max_intensity = backlight_get_intensity();

    // Define intensity levels
    const uint16_t dim_intensity = (BACKLIGHT_BRIGHTNESS_MAX * (uint32_t)10) / 100;  // 10% for darkness

    // Get configurable thresholds from preferences
    const uint32_t zone1_upper_bound = backlight_get_dynamic_min_threshold();  // Upper bound of Zone 1 (utter darkness)
    const uint32_t zone2_upper_bound = ambient_light_get_dark_threshold();     // Upper bound of Zone 2 (reuses existing dark threshold)

    // 3-Zone Algorithm using multiple samples:
    // Zone 1: ALL samples <= zone1_upper_bound (utter darkness) -> 10% brightness (dim but readable)
    // Only use low intensity if ALL samples confirm we're in the dark (robust against noise)
    if (prv_all_samples_below_or_equal(zone1_upper_bound)) {
      return dim_intensity;
    }

    // For Zone 2/3 decision, use average of samples
    uint32_t als_average = prv_get_als_average();

    // Zone 2: ALS average (zone1_upper_bound+1) to zone2_upper_bound (dim/indoor light) -> user max brightness
    if (als_average <= zone2_upper_bound) {
      return user_max_intensity;
    }

    // Zone 3: ALS average > zone2_upper_bound (bright outdoor) -> OFF (handled in prv_light_allowed)
    // Fallback return if somehow we get here (prv_light_allowed should prevent this)
    return user_max_intensity;
  }
#endif
  
  return backlight_get_intensity();
}

static void prv_update_intensity_analytics(uint8_t new_intensity_pct) {
  RtcTicks now_ticks = rtc_get_ticks();

  // Calculate time delta in ms since last sample
  if (s_last_intensity_sample_ticks > 0) {
    uint32_t time_delta_ms = ((now_ticks - s_last_intensity_sample_ticks) * 1000) / RTC_TICKS_HZ;

    // Accumulate intensity × time for weighted average calculation
    // Use last sampled intensity for the period that just elapsed
    s_intensity_time_product_sum += (uint64_t)s_last_sampled_intensity_pct * time_delta_ms;

    // Track total on-time when intensity is above zero
    if (s_last_sampled_intensity_pct > 0) {
      s_total_on_time_ms += time_delta_ms;
    }
  }

  // Update tracking variables
  s_last_intensity_sample_ticks = now_ticks;
  s_last_sampled_intensity_pct = new_intensity_pct;
}

static void prv_change_brightness(int32_t new_brightness) {
  // Use fade start intensity during fading, otherwise get current intensity
  uint16_t reference_intensity = (s_light_state == LIGHT_STATE_ON_FADING && s_fade_start_intensity > 0)
                                  ? s_fade_start_intensity
                                  : prv_backlight_get_intensity();
  const uint16_t HALF_BRIGHTNESS = (reference_intensity - BACKLIGHT_BRIGHTNESS_OFF) / 2;

  // update the debug stats
  if (new_brightness > HALF_BRIGHTNESS && s_current_brightness <= HALF_BRIGHTNESS) {
    // getting brighter and have now transitioned past half brightness
    PBL_ANALYTICS_TIMER_START(backlight_on_time_ms);
  }

  if (new_brightness <= HALF_BRIGHTNESS && s_current_brightness > HALF_BRIGHTNESS) {
    // getting dimmer and have now transitioned past half brightness
    PBL_ANALYTICS_TIMER_STOP(backlight_on_time_ms);
  }

  // Track intensity for analytics (convert to percentage)
  uint8_t new_intensity_pct = (new_brightness * 100) / BACKLIGHT_BRIGHTNESS_MAX;
  prv_update_intensity_analytics(new_intensity_pct);

  backlight_set_brightness(new_brightness);
  s_current_brightness = new_brightness;
}

static void prv_change_state(BacklightState new_state) {
  BacklightState old_state = s_light_state;
  s_light_state = new_state;

  // Take multiple ALS samples when transitioning from OFF to ON states
  // This prevents feedback from the backlight illuminating the sensor
  // and provides robust readings to detect utter darkness (Zone 1)
  if ((new_state == LIGHT_STATE_ON || new_state == LIGHT_STATE_ON_TIMED) &&
      s_current_brightness == BACKLIGHT_BRIGHTNESS_OFF) {
    prv_sample_als_multiple_times();
  }

  // Calculate the new brightness and reset any timers based on our state.
  int32_t new_brightness = 0;
  
  switch (new_state) {
    case LIGHT_STATE_ON:
      new_brightness = prv_backlight_get_intensity();
      new_timer_stop(s_timer_id);
      break;
    case LIGHT_STATE_ON_TIMED:
      new_brightness = prv_backlight_get_intensity();

      // Schedule the timer to move us from the ON_TIMED state to the ON_FADING state
      new_timer_start(s_timer_id, backlight_get_timeout_ms(),
                      light_timer_callback, NULL, 0 /* flags */);
      break;
    case LIGHT_STATE_ON_FADING:
      // Capture the starting intensity only when we first enter fading state
      if (old_state != LIGHT_STATE_ON_FADING) {
        s_fade_start_intensity = s_current_brightness;
        s_fade_step_size = s_fade_start_intensity / LIGHT_FADE_STEPS;
      }
      new_brightness = s_current_brightness - s_fade_step_size;

      if (new_brightness <= BACKLIGHT_BRIGHTNESS_OFF) {
        // Done fading!
        new_brightness = BACKLIGHT_BRIGHTNESS_OFF;
        s_light_state = LIGHT_STATE_OFF;

        // Don't need to cancel the timer, we can only get here from the just-expired timer.
      } else {
        // Reschedule the timer so we step down the brightness again.
        new_timer_start(s_timer_id, LIGHT_FADE_TIME_MS / LIGHT_FADE_STEPS, light_timer_callback, NULL, 0 /* flags */);
      }
      break;
    case LIGHT_STATE_OFF:
      new_brightness = BACKLIGHT_BRIGHTNESS_OFF;
      new_timer_stop(s_timer_id);
      break;
  }

  if (s_current_brightness != new_brightness) {
    prv_change_brightness(new_brightness);
  }
}

static bool prv_light_allowed(void) {
  if (!s_backlight_allowed) {
    return false;
  }
  
  if (backlight_is_enabled()) {
    if (backlight_is_ambient_sensor_enabled()) {
      // If the light is off and it's bright outside, don't allow the light to turn on
      // (we don't need it!). Grab the mutex here so that the timer state machine doesn't change
      // the light brightness while we're checking the ambient light levels.
      bool allowed = !((s_current_brightness == 0) && ambient_light_is_light());
      return allowed;
    } else {
      return true;
    }
  } else {
    return false;
  }
}

void light_init(void) {
  s_light_state = LIGHT_STATE_OFF;
  s_current_brightness = BACKLIGHT_BRIGHTNESS_OFF;
  s_timer_id = new_timer_create();
  s_num_buttons_down = 0;
  s_user_controlled_state = false;
  s_fade_start_intensity = 0;
  s_fade_step_size = 0;
  s_mutex = mutex_create();

  // Initialize ALS sample buffer
  s_als_sample_count = 0;
  for (uint8_t i = 0; i < ALS_SAMPLE_BUFFER_SIZE; i++) {
    s_als_sample_buffer[i] = 0;
  }

  // Initialize intensity analytics tracking
  s_intensity_time_product_sum = 0;
  s_last_intensity_sample_ticks = 0;
  s_last_sampled_intensity_pct = 0;
  s_total_on_time_ms = 0;
}

void light_button_pressed(void) {
  mutex_lock(s_mutex);

  s_num_buttons_down++;
  if (s_num_buttons_down > 4) {
    PBL_LOG_ERR("More buttons were pressed than have been released.");
    s_num_buttons_down = 0;
  }

  // set the state to be on; releasing buttons will start the timer counting down
  if (prv_light_allowed()) {
    prv_change_state(LIGHT_STATE_ON);
  }

  mutex_unlock(s_mutex);
}

void light_button_released(void) {
  mutex_lock(s_mutex);

  s_num_buttons_down--;
  if (s_num_buttons_down < 0) {
    PBL_LOG_ERR("More buttons were released than have been pressed.");
    s_num_buttons_down = 0;
  }

  if (s_num_buttons_down == 0 &&
      s_light_state == LIGHT_STATE_ON &&
      !s_user_controlled_state) {
    // no more buttons pressed: wait for a bit and then start the fade-out timer
    prv_change_state(LIGHT_STATE_ON_TIMED);
  }

  mutex_unlock(s_mutex);
}

void light_enable_interaction(void) {
  mutex_lock(s_mutex);

  //if some buttons are held or light_enable is asserted, do nothing
  if (s_num_buttons_down > 0 || s_light_state == LIGHT_STATE_ON) {
    mutex_unlock(s_mutex);
    return;
  }

  if (prv_light_allowed()) {
    prv_change_state(LIGHT_STATE_ON_TIMED);
  } else {
    PBL_LOG_INFO("Backlight rejected: allowed=%d, brightness=%ld, is_light=%d",
                 s_backlight_allowed, s_current_brightness, ambient_light_is_light());
  }

  mutex_unlock(s_mutex);
}

void light_enable(bool enable) {
  mutex_lock(s_mutex);

  // This function is a bit of a black sheep - it dives in and messes with the normal
  // flow of the state machine.
  // We don't actually use it, but it is now documented and used in the SDK, so
  // I am reluctant to chop it out.

  s_user_controlled_state = enable;

  if (enable) {
    prv_change_state(LIGHT_STATE_ON);
  } else if (s_num_buttons_down == 0) {
    // reset the state if someone calls light_enable(false);
    // (unless there are buttons pressed, then leave the backlight on)
    prv_change_state(LIGHT_STATE_OFF);
  }

  mutex_unlock(s_mutex);
}

void light_enable_respect_settings(bool enable) {
  mutex_lock(s_mutex);

  s_user_controlled_state = enable;

  if (enable) {
    if (prv_light_allowed()) {
      prv_change_state(LIGHT_STATE_ON);
    }
  } else if (s_num_buttons_down == 0) {
    prv_change_state(LIGHT_STATE_OFF);
  }

  mutex_unlock(s_mutex);
}

void light_reset_user_controlled(void) {
  mutex_lock(s_mutex);

  // http://www.youtube.com/watch?v=6t_KgE6Yuqg
  if (s_user_controlled_state) {
    s_user_controlled_state = false;

    if (s_num_buttons_down == 0) {
      prv_change_state(LIGHT_STATE_OFF);
    }
  }

  mutex_unlock(s_mutex);
}

static void prv_light_reset_to_timed_mode(void) {
  mutex_lock(s_mutex);

  if (s_user_controlled_state) {
    s_user_controlled_state = false;
    if (prv_light_allowed()) {
      prv_change_state(LIGHT_STATE_ON_TIMED);
    }
  }

  mutex_unlock(s_mutex);
}

void light_toggle_enabled(void) {
  mutex_lock(s_mutex);

  backlight_set_enabled(!backlight_is_enabled());
  if (prv_light_allowed()) {
    prv_change_state(LIGHT_STATE_ON_TIMED);
  } else {
    prv_change_state(LIGHT_STATE_OFF);
  }
  mutex_unlock(s_mutex);
}

void light_toggle_ambient_sensor_enabled(void) {
  mutex_lock(s_mutex);
  backlight_set_ambient_sensor_enabled(!backlight_is_ambient_sensor_enabled());
  if (prv_light_allowed() && !ambient_light_is_light()) {
    prv_change_state(LIGHT_STATE_ON_TIMED);
  } else {
    prv_change_state(LIGHT_STATE_OFF);
    // FIXME: PBL-24793 There is an edge case of when the backlight has timed off
    // or you're toggling it from no ambient (always light on buttons) to ambient,
    // you will see it turn on and immediately off if its bright out
  }
  mutex_unlock(s_mutex);
}

void light_allow(bool allowed) {
  if (s_backlight_allowed && !allowed) {
    prv_change_state(LIGHT_STATE_OFF);
  }
  s_backlight_allowed = allowed;
}

DEFINE_SYSCALL(void, sys_light_enable_interaction, void) {
  light_enable_interaction();
}

DEFINE_SYSCALL(void, sys_light_enable, bool enable) {
  light_enable(enable);
}

DEFINE_SYSCALL(void, sys_light_enable_respect_settings, bool enable) {
  light_enable_respect_settings(enable);
}

DEFINE_SYSCALL(void, sys_light_reset_to_timed_mode, void) {
  prv_light_reset_to_timed_mode();
}

extern BacklightBehaviour backlight_get_behaviour(void);

uint8_t light_get_current_brightness_percent(void) {
  uint8_t percent = (s_current_brightness * 100) / BACKLIGHT_BRIGHTNESS_MAX;
  return percent;
}

void pbl_analytics_external_collect_backlight_stats(void) {
  mutex_lock(s_mutex);

  // Capture one final sample to account for time since last brightness change
  uint8_t current_intensity_pct = (s_current_brightness * 100) / BACKLIGHT_BRIGHTNESS_MAX;
  prv_update_intensity_analytics(current_intensity_pct);

  // Calculate time-weighted average intensity using internally tracked on-time
  uint32_t avg_intensity_pct = 0;
  if (s_total_on_time_ms > 0) {
    // Calculate weighted average: sum(intensity × time) / total_time
    avg_intensity_pct = s_intensity_time_product_sum / s_total_on_time_ms;
  }

  PBL_ANALYTICS_SET_UNSIGNED(backlight_avg_intensity_pct, avg_intensity_pct);

  // Reset accumulators for next period
  s_intensity_time_product_sum = 0;
  s_total_on_time_ms = 0;
  s_last_intensity_sample_ticks = rtc_get_ticks();

  mutex_unlock(s_mutex);
}
