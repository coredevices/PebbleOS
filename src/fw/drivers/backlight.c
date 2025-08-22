/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "drivers/backlight.h"

#include <string.h>
#include <stdlib.h>

#include "board/board.h"
#include "debug/power_tracking.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/led_controller.h"
#include "drivers/periph_config.h"
#include "drivers/pwm.h"
#include "drivers/timer.h"
#include "kernel/util/stop.h"
#include "system/logging.h"
#include "system/passert.h"

// Parameters to a timer based PWM.
//
//  ----------------     ----------     ----------     ----------      ----------
//                 |     |        |     |        |     |        |      |
//                 |     |        |     |        |     |        |      |
//                 |     |        |     |        |     |        |      |
//                 |     |        |     |        |     |        |      |
//                 |     |        |     |        |     |        |      |
//                 |     |        |     |        |     |        |      |
//                 |     |        |     |        |     |        |      |
//                 -------        -------        -------        --------
//
// The resulting waveform has a frequency of PWM_OUTPUT_FREQUENCY_HZ. Inside each period, the timer
// counts up to TIMER_PERIOD_RESOLUTION. This means the counter increments at a rate of
// PWM_OUTPUT_FREQUENCY_HZ * TIMER_PERIOD_RESOLUTION, which is the frequency that our timer
// prescalar has to calculate. The duty cycle is defined by the TIM_Pulse parameter, which
// controls after which counter value the output waveform will become active. For example, a
// TIM_Pulse value of TIMER_PERIOD_RESOLUTION / 4 will result in an output waveform that will go
// active after 25% of it's period has elapsed.

//! The counter reload value. The timer will count from 0 to this value and then reset again.
//! The TIM_Pulse member below controls for how many of these counts the resulting PWM signal is
//! active for.
static const uint32_t TIMER_PERIOD_RESOLUTION = 1024;

//! The number of periods we have per second.
//! Note that we want BOARD_CONFIG_BACKLIGHT.timer.peripheral to have as short a period as
//! possible for power reasons.
static const uint32_t PWM_OUTPUT_FREQUENCY_HZ = 256;

static bool s_initialized = false;

static bool s_backlight_pwm_enabled = false;

//! Bitmask of who wants to hold the LED enable on.
//! see \ref led_enable, \ref led_disable, \ref LEDEnabler
static uint32_t s_led_enable;

static void prv_backlight_pwm_enable(bool on) {
  pwm_enable(&BOARD_CONFIG_BACKLIGHT.pwm, on);

  if (on != s_backlight_pwm_enabled) {
    if (on) {
      stop_mode_disable(InhibitorBacklight);
    } else {
      stop_mode_enable(InhibitorBacklight);
    }
  }

  s_backlight_pwm_enabled = on;
}

void backlight_init(void) {
  if (s_initialized) {
    return;
  }

  s_led_enable = 0;

  if (BOARD_CONFIG_BACKLIGHT.options & ActuatorOptions_Ctl) {
    periph_config_acquire_lock();
    gpio_output_init(&BOARD_CONFIG_BACKLIGHT.ctl, GPIO_OType_PP, GPIO_Speed_2MHz);
    gpio_output_set(&BOARD_CONFIG_BACKLIGHT.ctl, false);
    periph_config_release_lock();
    s_initialized = true;
  }

  if (BOARD_CONFIG_BACKLIGHT.options & ActuatorOptions_Pwm) {
    periph_config_acquire_lock();
    pwm_init(&BOARD_CONFIG_BACKLIGHT.pwm,
             TIMER_PERIOD_RESOLUTION,
             TIMER_PERIOD_RESOLUTION * PWM_OUTPUT_FREQUENCY_HZ);
    periph_config_release_lock();
    s_initialized = true;
  }

  if (BOARD_CONFIG_BACKLIGHT.options & ActuatorOptions_IssiI2C) {
    led_controller_init();
    s_initialized = true;
  }
}

// TODO: PBL-36077 Move to a generic 4v5 enable
void led_enable(LEDEnabler enabler) {
  if (s_led_enable == 0) {
    gpio_output_set(&BOARD_CONFIG_BACKLIGHT.ctl, true);
  }
  s_led_enable |= enabler;
}

// TODO: PBL-36077 Move to a generic 4v5 disable
void led_disable(LEDEnabler enabler) {
  s_led_enable &= ~enabler;
  if (s_led_enable == 0) {
    gpio_output_set(&BOARD_CONFIG_BACKLIGHT.ctl, false);
  }
}

void backlight_set_brightness(uint16_t brightness) {
  if (BOARD_CONFIG_BACKLIGHT.options & ActuatorOptions_Ctl) {
    if (brightness == 0) {
      led_disable(LEDEnablerBacklight);
    } else {
      led_enable(LEDEnablerBacklight);
    }
  }

  if (BOARD_CONFIG_BACKLIGHT.options & ActuatorOptions_Pwm) {
    if (brightness == 0) {
      if (s_backlight_pwm_enabled) {
        prv_backlight_pwm_enable(false);
      }
      PWR_TRACK_BACKLIGHT("OFF", PWM_OUTPUT_FREQUENCY_HZ, 0);
    } else {
      if (!s_backlight_pwm_enabled) {
        prv_backlight_pwm_enable(true);
      }

      // By setting higher values in the TIM_Pulse register, we're causing the output waveform
      // to be low for a longer period of time, which causes the backlight to be brighter.
      //
      // The brightness value has a range of 0 to 0x3fff which is 2^15. The period of the timer
      // counter is 2^10. We want to rescale the brightness range into a subset of the timer
      // counter range. Different boards will have a different duty cycle that represent the
      // "fully on" state.
      const uint32_t pwm_scaling_factor = BACKLIGHT_BRIGHTNESS_MAX / TIMER_PERIOD_RESOLUTION;
      const uint32_t desired_duty_cycle = brightness * BOARD_CONFIG.backlight_max_duty_cycle_percent
                                          / pwm_scaling_factor / 100;
      pwm_set_duty_cycle(&BOARD_CONFIG_BACKLIGHT.pwm, desired_duty_cycle);
      PWR_TRACK_BACKLIGHT("ON", PWM_OUTPUT_FREQUENCY_HZ,
                          (desired_duty_cycle * 100) / TIMER_PERIOD_RESOLUTION);
    }
  }

  if (BOARD_CONFIG_BACKLIGHT.options & ActuatorOptions_IssiI2C) {
    led_controller_backlight_set_brightness(brightness * 100 / BACKLIGHT_BRIGHTNESS_MAX);
  }
}

void command_backlight_ctl(const char *arg) {
  const int bright_percent = atoi(arg);
  if (bright_percent < 0 || bright_percent > 100) {
    prompt_send_response("Invalid Brightness");
    return;
  }
  backlight_set_brightness((BACKLIGHT_BRIGHTNESS_MAX * bright_percent) / 100);
  prompt_send_response("OK");
}
