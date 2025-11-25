/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/vibe.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "drivers/pwm.h"
#include "drivers/timer.h"
#include "kernel/util/stop.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

#include "services/common/analytics/analytics.h"
#include "services/common/battery/battery_monitor.h"
#include "services/common/battery/battery_state.h"
#include "services/common/analytics/analytics.h"

#include <string.h>


//! Make a resolution of 100. Working in integer duty cycles on the following ranges:
//!
//! For a 2-direction, rotating vibe (PWM actuates an H-Bridge):
//!   [0, 49] : Full-strength reverse rotation to zero-strength reverse rotation.
//!   50 : No rotation strength
//!   [51, 100] : Zero-strength forward rotation to full-strength forward rotation.
//!
//! For a 1-direction vibe:
//!   0 : No vibration strength.
//!   [1, 100] : Zero strength vibration to full-strength vibration.
//!
//! This must be an even value so that a half-way point exists as an edge between an equal number of
//! clock cycles on either side.
#define PWM_TIMER_UPDATE_PERIOD (100)

// Operating frequency of DRV2603 is in the [10, 250] kHz range.
#define PWM_OUTPUT_FREQUENCY_HZ (22 * 1000)

// Count clock needs to run at least as fast as the (update period * output frequency)
#define PWM_TIMER_FREQUENCY_HZ (PWM_TIMER_UPDATE_PERIOD * PWM_OUTPUT_FREQUENCY_HZ)

// 50% duty cycle means not vibrating.
#define PWM_DUTY_CYCLE_OFF (PWM_TIMER_UPDATE_PERIOD / 2)
#define PWM_DUTY_CYCLE_FULL (PWM_TIMER_UPDATE_PERIOD)

static uint8_t s_vibe_duty_cycle = PWM_DUTY_CYCLE_FULL;
static bool s_initialized = false;

void vibe_init(void) {
  if (s_initialized) {
    return;
  }

  periph_config_acquire_lock();

  if (BOARD_CONFIG_VIBE.options & ActuatorOptions_Ctl) {
    gpio_output_init(&BOARD_CONFIG_VIBE.ctl, GPIO_OType_PP, GPIO_Speed_2MHz);
    gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
    s_initialized = true;
  }

  if (BOARD_CONFIG_VIBE.options & ActuatorOptions_Pwm) {
    pwm_init(&BOARD_CONFIG_VIBE.pwm, PWM_TIMER_UPDATE_PERIOD, PWM_TIMER_FREQUENCY_HZ);
    s_initialized = true;
  }

  if (BOARD_CONFIG_VIBE.options & ActuatorOptions_HBridge) {
    PBL_ASSERTN(BOARD_CONFIG_VIBE.options & ActuatorOptions_Pwm);
  }

  periph_config_release_lock();
}

//! Enables / disables the PWM timer used for vibe control.
//! Note: assumes the timer peripheral is enabled
static void prv_vibe_pwm_enable(bool on) {
  pwm_enable(&BOARD_CONFIG_VIBE.pwm, on);

  static bool stop_mode_disabled = false;
  if (stop_mode_disabled != on) {
    if (on) {
      stop_mode_disable(InhibitorVibes);
    } else {
      stop_mode_enable(InhibitorVibes);
    }
    stop_mode_disabled = on;
  }
}

static uint16_t prv_get_vsys_mv(void) {
  if (battery_get_charge_state().is_plugged) {
    // Plugged in, use Vsys rather than Vbat
    return pmic_get_vsys();
  }
  // Not plugged in, use latest battery reading
  return battery_state_get_voltage();
}

static uint32_t prv_vibe_get_pwm_duty_cycle(int8_t strength) {
  if (BOARD_CONFIG_VIBE.options & ActuatorOptions_HBridge) {
    // Scale from -100..100 (strength) to 0..100 (duty cycle)
    return ((PWM_DUTY_CYCLE_FULL - PWM_DUTY_CYCLE_OFF) * strength / VIBE_STRENGTH_MAX)
              + PWM_DUTY_CYCLE_OFF;
  } else {
    // Treat "reverse" rotation strength as if it were "forward" strength.
    uint32_t duty_cycle = ABS(strength);

    // Scale the duty cycle given the current battery voltage
    if (BOARD_CONFIG_VIBE.vsys_scale > 0) {
      const uint16_t vsys_mv = prv_get_vsys_mv();
      PBL_ASSERTN(vsys_mv > 0);
      duty_cycle = (BOARD_CONFIG_VIBE.vsys_scale * duty_cycle) / vsys_mv;
    }
    return duty_cycle;
  }
}

static void prv_vibe_raw_ctl(bool on) {
  if (BOARD_CONFIG_VIBE.options & ActuatorOptions_Pwm) {
    const uint32_t duty_cycle = (on) ? s_vibe_duty_cycle : PWM_DUTY_CYCLE_OFF;
    prv_vibe_pwm_enable(on);
    pwm_set_duty_cycle(&BOARD_CONFIG_VIBE.pwm, s_vibe_duty_cycle);
  }

  if (BOARD_CONFIG_VIBE.options & ActuatorOptions_Ctl) {
    gpio_output_set(&BOARD_CONFIG_VIBE.ctl, on);
  }
}

void vibe_set_strength(int8_t strength) {
  uint8_t duty_cycle = prv_vibe_get_pwm_duty_cycle(strength);
  s_vibe_duty_cycle = MIN(duty_cycle, PWM_DUTY_CYCLE_FULL);
}

void vibe_ctl(bool on) {
  if (!s_initialized) {
    return;
  }

  if (on && battery_monitor_critical_lockout()) {
    on = false;
  }

  static bool s_on = false;
  if (on && !s_on) {
    analytics_inc(ANALYTICS_DEVICE_METRIC_VIBRATOR_ON_COUNT, AnalyticsClient_System);
    analytics_stopwatch_start(ANALYTICS_APP_METRIC_VIBRATOR_ON_TIME, AnalyticsClient_App);
    analytics_stopwatch_start(ANALYTICS_DEVICE_METRIC_VIBRATOR_ON_TIME, AnalyticsClient_System);
  } else if (!on && s_on) {
    analytics_stopwatch_stop(ANALYTICS_APP_METRIC_VIBRATOR_ON_TIME);
    analytics_stopwatch_stop(ANALYTICS_DEVICE_METRIC_VIBRATOR_ON_TIME);
  }
  s_on = on;

  PBL_LOG(LOG_LEVEL_DEBUG, "Vibe status <%s>", on ? "on" : "off");

  prv_vibe_raw_ctl(on);
}

void vibe_force_off(void) {
  if (!s_initialized) {
    return;
  }

  prv_vibe_raw_ctl(false);
}

int8_t vibe_get_braking_strength(void) {
  if (BOARD_CONFIG_VIBE.options & ActuatorOptions_HBridge) {
    // We support the full -100..100 range, send it all the way backwards
    return VIBE_STRENGTH_MIN;
  } else {
    // We only support the 0..100 range, just ask it to turn off
    return VIBE_STRENGTH_OFF;
  }
}

status_t vibe_calibrate(void) {
  return E_INVALID_OPERATION;
}

void command_vibe_ctl(const char *arg) {
  int strength = atoi(arg);

  const bool out_of_bounds = ((strength < 0) || (strength > VIBE_STRENGTH_MAX));
  const bool not_a_number = (strength == 0 && arg[0] != '0');
  if (out_of_bounds || not_a_number) {
    prompt_send_response("Invalid argument");
    return;
  }

  vibe_set_strength(strength);

  const bool turn_on = strength != 0;
  vibe_ctl(turn_on);
  prompt_send_response("OK");
}
