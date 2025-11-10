/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/gpio.h"
#include "drivers/led_controller.h"
#include "kernel/util/delay.h"
#include "system/logging.h"
#include "util/math.h"

// AW9364E 1-wire dimming protocol implementation
// The AW9364E uses pulse counting for brightness control:
// - 1 pulse = 20mA (brightest)
// - 16 pulses = 1.25mA (dimmest)
// Timing: THI > 0.5us, 0.5us < TLO < 500μs
// Shutdown: EN low for > 2.5ms

#define AW9364E_TON_US 20U
#define AW9364E_THI_US 1U
#define AW9364E_TLO_US 1U
#define AW9364E_MAX_PULSES 16U
#define AW9364E_OFF_TIME_US 2600U

void led_controller_init(void) {
  gpio_output_init(&AW9364E.gpio, GPIO_OType_PP, GPIO_Speed_2MHz);
}

void led_controller_backlight_set_brightness(uint8_t brightness) {
  uint8_t pulse_count;

  if (brightness > 100) {
    brightness = 100;
  }

  pulse_count = AW9364E_MAX_PULSES - DIVIDE_CEIL(brightness * AW9364E_MAX_PULSES, 100U) + 1U;

  gpio_output_set(&AW9364E.gpio, false);
  delay_us(AW9364E_OFF_TIME_US);

  if (pulse_count > AW9364E_MAX_PULSES) {
    return;
  }

  for (uint8_t i = 0U; i < pulse_count; i++) {
    gpio_output_set(&AW9364E.gpio, false);
    delay_us(AW9364E_TLO_US);
    gpio_output_set(&AW9364E.gpio, true);
    delay_us(i == 0U ? AW9364E_TON_US : AW9364E_THI_US);
  }
}

void led_controller_rgb_set_color(uint32_t rgb_color) {}

uint32_t led_controller_rgb_get_color(void) {
  return 0UL;
}

void command_rgb_set_color(const char *color) {
  uint32_t color_val = strtol(color, NULL, 16);

  led_controller_rgb_set_color(color_val);
}
