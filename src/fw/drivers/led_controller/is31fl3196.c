/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/led_controller.h"

#include "board/board.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "system/logging.h"
#include "system/passert.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#include <mcu.h>

enum {
  RegShutdown = 0x00,
  RegLedCtrl = 0x01,
  RegConfig1 = 0x03,
  RegConfig2 = 0x04,
  RegRampingMode = 0x05,
  RegBreathingMark = 0x06,
  RegPwmOut1 = 0x07,
  RegPwmOut2 = 0x08,
  RegPwmOut3 = 0x09,
  RegPwmOut4 = 0x0a,
  RegPwmOut5 = 0x0b,
  RegPwmOut6 = 0x0c,
  RegDataUpdate = 0x10,
  RegT0Out1 = 0x11,
  RegT0Out2 = 0x12,
  RegT0Out3 = 0x13,
  RegT0Out4 = 0x14,
  RegT0Out5 = 0x15,
  RegT0Out6 = 0x16,
  RegT1T3Rgb1 = 0x1a,
  RegT1T3Rgb2 = 0x1b,
  RegT4Out1 = 0x1d,
  RegT4Out2 = 0x1e,
  RegT4Out3 = 0x1f,
  RegT4Out4 = 0x20,
  RegT4Out5 = 0x21,
  RegT4Out6 = 0x22,
  RegTimeUpdate = 0x26,
  RegReset = 0xff
};

static bool s_backlight_off;
static bool s_initialized = false;
static uint32_t s_rgb_current_color = LED_BLACK;

static bool write_register(uint8_t register_address, uint8_t value) {
  return i2c_write_register(I2C_LED, register_address, value);
}

// used to bring hardware shutdown pin on led controller low, this brings down our shutdown current
static void prv_shutdown(bool shutdown) {
  periph_config_acquire_lock();
  gpio_output_set(&BOARD_CONFIG_BACKLIGHT.ctl, !shutdown);
  periph_config_release_lock();
}

static void prv_init_pins(void) {
  periph_config_acquire_lock();
  gpio_output_init(&BOARD_CONFIG_BACKLIGHT.ctl, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&BOARD_CONFIG_BACKLIGHT.ctl, false);
  periph_config_release_lock();
}

void led_controller_init(void) {
  PBL_ASSERTN(BOARD_CONFIG_BACKLIGHT.options & BacklightOptions_LedController);

  prv_init_pins();
  prv_shutdown(false);

  i2c_use(I2C_LED);

  // reset the LED controller
  if (!write_register(RegReset, 0xaa)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LED Controller is MIA");
    goto cleanup;
  } else {
    s_initialized = true;
  }

  // take the led controller out of shutdown
  write_register(RegShutdown, 0x01);

  // set config1 to 0x00 (PWM, Audio disable, AGC enable, AGC fast mode)
  write_register(RegConfig1, 0x00);

  // set config2 to 0x40 (master control, 25mA drive, 0dB gain)
  write_register(RegConfig2, 0x70);

  // set ramping_mode to 0x00 (disable ramping)
  // TODO: this is potentially quite useful for us
  write_register(RegRampingMode, 0x00);

  // set breathing mark to 0x00 (disable breathing)
  // TODO: this is potentially quite useful for us for the RGB LEDs
  write_register(RegBreathingMark, 0x00);

  s_backlight_off = true;
  s_rgb_current_color = LED_BLACK;

cleanup:
  i2c_release(I2C_LED);
  prv_shutdown(true);
}

void led_controller_backlight_set_brightness(uint8_t brightness) {
  if ((BOARD_CONFIG_BACKLIGHT.options & BacklightOptions_LedController) == 0 || !s_initialized) {
    return;
  }

  prv_shutdown(false);
  i2c_use(I2C_LED);

  write_register(RegPwmOut1, brightness);
  write_register(RegPwmOut2, brightness);
  write_register(RegPwmOut3, brightness);

  write_register(RegDataUpdate, 0xaa);

  i2c_release(I2C_LED);

  s_backlight_off = (brightness == 0);

  if (s_backlight_off && s_rgb_current_color == LED_BLACK) {
    prv_shutdown(true);
  }
}


void led_controller_rgb_set_color(uint32_t rgb_color) {
  if ((BOARD_CONFIG_BACKLIGHT.options & BacklightOptions_LedController) == 0 || !s_initialized) {
    return;
  }

  s_rgb_current_color = rgb_color;

  uint8_t red = (s_rgb_current_color & 0x00FF0000) >> 16;
  uint8_t green = (s_rgb_current_color & 0x0000FF00) >> 8;
  uint8_t blue = (s_rgb_current_color & 0x000000FF);

  prv_shutdown(false);
  i2c_use(I2C_LED);

  write_register(RegPwmOut4, red);
  write_register(RegPwmOut6, blue);
  write_register(RegPwmOut5, green);

  write_register(RegDataUpdate, 0xaa);

  i2c_release(I2C_LED);

  if (s_backlight_off && s_rgb_current_color == LED_BLACK) {
    prv_shutdown(true);
  }
}

uint32_t led_controller_rgb_get_color(void) {
  return s_rgb_current_color;
}

void command_rgb_set_color(const char* color) {
  uint32_t color_val = strtol(color, NULL, 16);

  led_controller_rgb_set_color(color_val);
}

