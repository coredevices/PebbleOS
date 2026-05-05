/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/light.h"

#include "board/board.h"
#include "drivers/ambient_light.h"
#include "drivers/backlight.h"
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
#include "drivers/backlight.h"
#endif
#include "drivers/rtc.h"
#include "kernel/low_power.h"
#include "pbl/services/analytics/analytics.h"
#include "pbl/services/battery/battery_monitor.h"
#include "pbl/services/new_timer/new_timer.h"
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
const uint8_t LIGHT_FADE_STEPS = 20;

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

//! The brightness of the display in a range between 0 and 100
static uint8_t s_current_brightness;

//! Timer to count down from the LIGHT_STATE_ON_TIMED state.
static TimerID s_timer_id;

//! Refcount of the number of buttons that are currently pushed
static int s_num_buttons_down;

//! The current app is forcing the light on and off, don't muck with it.
static bool s_user_controlled_state;

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
//! The app's requested backlight tint. Valid only when s_app_rgb_override_valid
//! is true; otherwise the LED uses the user default (white).
static uint32_t s_app_rgb_override;
static bool s_app_rgb_override_valid;

//! Count of active modal preempts (notifications, etc.) that temporarily
//! mask any app RGB override. The app's override stays stored; only while
//! this refcount is non-zero is the LED driven to the default color.
//! When the refcount returns to zero, the override (if any) is re-applied.
static uint8_t s_color_preempt_refcount;
#endif

//! For temporary disabling backlight (ie: low power mode)
static bool s_backlight_allowed = false;

//! Starting intensity for fade-out (captured when fade begins)
static uint8_t s_fade_start_intensity = 0;

//! Fade step size (calculated once at start of fade to avoid rounding jitter)
static uint8_t s_fade_step_size = 0;

//! Mutex to guard all the above state. We have a pattern of taking the lock in the public functions and assuming
//! it's already taken in the prv_ functions.
static PebbleMutex *s_mutex;

//! Analytics: Track time-weighted average intensity
static uint64_t s_intensity_time_product_sum; // Sum of (intensity_pct × time_ms)
static RtcTicks s_last_intensity_sample_ticks; // Timestamp of last sample
static uint8_t s_last_sampled_intensity_pct; // Last intensity percentage sampled
static uint32_t s_total_on_time_ms; // Total backlight on time tracked internally

//! Short-lived cache so back-to-back ALS consumers in the same wake path
//! (prv_light_allowed → prv_backlight_get_intensity, plus a button release
//! that follows the press within the TTL) skip the ~200 ms I2C poll.
static uint32_t s_als_cached_level;
static RtcTicks s_als_cached_ticks;  // 0 = invalid
#define ALS_CACHE_TTL_TICKS (RTC_TICKS_HZ)  // 1 second

static void prv_change_state(BacklightState new_state);

static uint32_t prv_get_als_level(void) {
  RtcTicks now = rtc_get_ticks();
  const bool cache_valid =
      s_als_cached_ticks != 0 &&
      (s_current_brightness > 0 || (now - s_als_cached_ticks) < ALS_CACHE_TTL_TICKS);
  if (cache_valid) {
    return s_als_cached_level;
  }
  s_als_cached_level = ambient_light_get_light_level();
  s_als_cached_ticks = now;
  return s_als_cached_level;
}

static bool prv_als_is_light(void) {
  return prv_get_als_level() > ambient_light_get_dark_threshold();
}

static void light_timer_callback(void *data) {
  mutex_lock(s_mutex);
  prv_change_state(LIGHT_STATE_ON_FADING);
  mutex_unlock(s_mutex);
}

static uint8_t prv_backlight_get_intensity(void) {
  // low_power_mode backlight intensity (25% of max brightness)
  const uint8_t backlight_low_power_intensity = 25;
  
  if (low_power_is_active()) {
    return backlight_low_power_intensity;
  }
  
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT && !defined(RECOVERY_FW)
  // Dynamic backlight: dim in utter darkness, otherwise user max. The
  // bright-outdoor case is already filtered upstream by prv_light_allowed()
  // for ambient-sensor-enabled wakes; the few paths that bypass that gate
  // (app-driven force-on, ambient-sensor pref off) sensibly land at max here.
  if (backlight_is_dynamic_intensity_enabled()) {
    const uint8_t dim_intensity = 10;
    if (prv_get_als_level() <= backlight_get_dynamic_min_threshold()) {
      return dim_intensity;
    }
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

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
//! LED color to drive when no app has set an override. Backed by the
//! user's stored backlight-color preference, defaulting to BACKLIGHT_COLOR_WARM_WHITE.
static void prv_apply_rgb_color(void) {
  const bool preempted = (s_color_preempt_refcount > 0);
  const uint32_t color = (preempted || !s_app_rgb_override_valid)
                             ? backlight_get_default_color()
                             : s_app_rgb_override;
  backlight_set_color(color);
}
#endif

static void prv_change_brightness(uint8_t new_brightness) {
  // Scale the 0-100% to the maximum value allowed in hardware
  uint8_t scaled_brightness = (new_brightness * (uint16_t)BOARD_CONFIG.backlight_on_percent) / 100U;

  if (new_brightness == 0U) {
    PBL_ANALYTICS_TIMER_STOP(backlight_on_time_ms);
  } else {
    PBL_ANALYTICS_TIMER_START(backlight_on_time_ms);
  }

  prv_update_intensity_analytics(scaled_brightness);

  backlight_set_brightness(scaled_brightness);
  s_current_brightness = new_brightness;

#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  // backlight_set_brightness re-applies the last RGB color at the new
  // intensity; follow up with the app override (or default white) so the
  // LED reflects the current color request for this state change.
  prv_apply_rgb_color();
#endif
}

static void prv_change_state(BacklightState new_state) {
  BacklightState old_state = s_light_state;
  s_light_state = new_state;

  // Calculate the new brightness and reset any timers based on our state.
  uint8_t new_brightness = 0;

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
        if (s_fade_step_size == 0) {
          s_fade_step_size = 1;
        }
      }

      if (s_fade_step_size >= s_current_brightness) {
        new_brightness = 0;
        s_light_state = LIGHT_STATE_OFF;
      } else {
        new_brightness = s_current_brightness - s_fade_step_size;

        // Reschedule the timer so we step down the brightness again.
        new_timer_start(s_timer_id, LIGHT_FADE_TIME_MS / LIGHT_FADE_STEPS, light_timer_callback, NULL, 0 /* flags */);
      }
      break;
    case LIGHT_STATE_OFF:
      new_brightness = 0;
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
      bool allowed = !((s_current_brightness == 0) && prv_als_is_light());
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
  s_current_brightness = 0;
  s_timer_id = new_timer_create();
  s_num_buttons_down = 0;
  s_user_controlled_state = false;
  s_fade_start_intensity = 0;
  s_fade_step_size = 0;
  s_mutex = mutex_create();

  // Initialize intensity analytics tracking
  s_intensity_time_product_sum = 0;
  s_last_intensity_sample_ticks = 0;
  s_last_sampled_intensity_pct = 0;
  s_total_on_time_ms = 0;

  s_als_cached_level = 0;
  s_als_cached_ticks = 0;
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
    PBL_LOG_INFO("Backlight rejected: allowed=%d, brightness=%" PRIu8 ", is_light=%d",
                 s_backlight_allowed, s_current_brightness, prv_als_is_light());
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

void light_set_color_rgb888(uint32_t rgb) {
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  mutex_lock(s_mutex);
  s_app_rgb_override = rgb & 0x00FFFFFF;
  s_app_rgb_override_valid = true;
  if (s_light_state != LIGHT_STATE_OFF) {
    prv_apply_rgb_color();
  }
  mutex_unlock(s_mutex);
#else
  (void)rgb;
#endif
}

void light_set_system_color(void) {
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  mutex_lock(s_mutex);
  if (s_app_rgb_override_valid) {
    s_app_rgb_override_valid = false;
    if (s_light_state != LIGHT_STATE_OFF) {
      prv_apply_rgb_color();
    }
  }
  mutex_unlock(s_mutex);
#endif
}

void light_system_color_request(void) {
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  mutex_lock(s_mutex);
  if (s_color_preempt_refcount < UINT8_MAX) {
    s_color_preempt_refcount++;
  }
  if (s_color_preempt_refcount == 1 && s_light_state != LIGHT_STATE_OFF) {
    prv_apply_rgb_color();
  }
  mutex_unlock(s_mutex);
#endif
}

void light_system_color_release(void) {
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  mutex_lock(s_mutex);
  if (s_color_preempt_refcount > 0) {
    s_color_preempt_refcount--;
    if (s_color_preempt_refcount == 0 && s_light_state != LIGHT_STATE_OFF) {
      prv_apply_rgb_color();
    }
  }
  mutex_unlock(s_mutex);
#endif
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
  if (prv_light_allowed() && !prv_als_is_light()) {
    prv_change_state(LIGHT_STATE_ON_TIMED);
  } else {
    prv_change_state(LIGHT_STATE_OFF);
    // FIXME: PBL-24793 There is an edge case of when the backlight has timed off
    // or you're toggling it from no ambient (always light on buttons) to ambient,
    // you will see it turn on and immediately off if its bright out
  }
  mutex_unlock(s_mutex);
}

void light_toggle_dynamic_intensity_enabled(void) {
#if CAPABILITY_HAS_DYNAMIC_BACKLIGHT
  mutex_lock(s_mutex);
  backlight_set_dynamic_intensity_enabled(!backlight_is_dynamic_intensity_enabled());
  if (prv_light_allowed()) {
    prv_change_state(LIGHT_STATE_ON_TIMED);
  }
  mutex_unlock(s_mutex);
#endif
}

void light_allow(bool allowed) {
  if (s_backlight_allowed && !allowed) {
    prv_change_state(LIGHT_STATE_OFF);
  }
  s_backlight_allowed = allowed;
}

DEFINE_SYSCALL(bool, sys_light_is_on, void) {
  return light_is_on();
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

DEFINE_SYSCALL(void, sys_light_set_color_rgb888, uint32_t rgb) {
  light_set_color_rgb888(rgb);
}

DEFINE_SYSCALL(void, sys_light_set_system_color, void) {
  light_set_system_color();
}

extern BacklightBehaviour backlight_get_behaviour(void);

uint8_t light_get_current_brightness_percent(void) {
  return s_current_brightness;
}

bool light_is_on(void) {
  return s_light_state != LIGHT_STATE_OFF;
}

void pbl_analytics_external_collect_backlight_stats(void) {
  mutex_lock(s_mutex);

  // Capture one final sample to account for time since last brightness change
  prv_update_intensity_analytics(s_current_brightness);

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
