/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/led_controller.h"

#include "board/board.h"
#include "drivers/pwm.h"

static const uint32_t TIMER_PERIOD_RESOLUTION = 1024;
static const uint32_t PWM_OUTPUT_FREQUENCY_HZ = 256;

static uint32_t s_rgb_current_color;
static uint8_t s_brightness;

void led_controller_init(void) {
  pwm_init(&LED_CONTROLLER_PWM.pwm[0], TIMER_PERIOD_RESOLUTION,
           TIMER_PERIOD_RESOLUTION * PWM_OUTPUT_FREQUENCY_HZ);
  pwm_init(&LED_CONTROLLER_PWM.pwm[1], TIMER_PERIOD_RESOLUTION,
           TIMER_PERIOD_RESOLUTION * PWM_OUTPUT_FREQUENCY_HZ);
  pwm_init(&LED_CONTROLLER_PWM.pwm[2], TIMER_PERIOD_RESOLUTION,
           TIMER_PERIOD_RESOLUTION * PWM_OUTPUT_FREQUENCY_HZ);

  s_rgb_current_color = LED_CONTROLLER_PWM.initial_color;
}

void led_controller_backlight_set_brightness(uint8_t brightness) {
  if (brightness > 100) {
    brightness = 100;
  }

  if (s_brightness == brightness) {
    return;
  }

  s_brightness = brightness;

  if (s_brightness == 0) {
    pwm_enable(&LED_CONTROLLER_PWM.pwm[0], false);
    pwm_enable(&LED_CONTROLLER_PWM.pwm[1], false);
    pwm_enable(&LED_CONTROLLER_PWM.pwm[2], false);
    return;
  } else {
    pwm_enable(&LED_CONTROLLER_PWM.pwm[0], true);
    pwm_enable(&LED_CONTROLLER_PWM.pwm[1], true);
    pwm_enable(&LED_CONTROLLER_PWM.pwm[2], true);
    led_controller_rgb_set_color(s_rgb_current_color);
  }
}

void led_controller_rgb_set_color(uint32_t rgb_color) {
  s_rgb_current_color = rgb_color;

  if (s_brightness == 0) {
    return;
  }

  uint8_t r = (s_rgb_current_color & 0x00FF0000) >> 16;
  uint8_t g = (s_rgb_current_color & 0x0000FF00) >> 8;
  uint8_t b = (s_rgb_current_color & 0x000000FF);

  pwm_set_duty_cycle(&LED_CONTROLLER_PWM.pwm[0],
                     (r * TIMER_PERIOD_RESOLUTION * s_brightness) / 100 / UINT8_MAX);
  pwm_set_duty_cycle(&LED_CONTROLLER_PWM.pwm[1],
                     (g * TIMER_PERIOD_RESOLUTION * s_brightness) / 100 / UINT8_MAX);
  pwm_set_duty_cycle(&LED_CONTROLLER_PWM.pwm[2],
                     (b * TIMER_PERIOD_RESOLUTION * s_brightness) / 100 / UINT8_MAX);
}

uint32_t led_controller_rgb_get_color(void) { return s_rgb_current_color; }

void command_rgb_set_color(const char* color) {
  uint32_t color_val = strtol(color, NULL, 16);

  led_controller_rgb_set_color(color_val);
}