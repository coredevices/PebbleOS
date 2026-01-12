/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/led_controller.h"

#include "board/board.h"
#include "drivers/i2c.h"
#include "services/common/new_timer/new_timer.h"
#include "system/logging.h"
#include "system/passert.h"

#define AW2016_HEALTH_CHECK_INTERVAL_MS 100

#define AW2016_REG_RSTR 0x00U
#define AW2016_REG_RSTR_CHIP_ID 0x09U
#define AW2016_REG_RSTR_RST 0x55U

#define AW2016_REG_GCR1 0x01U
#define AW2016_REG_GCR1_CHGDIS_DIS (1U << 1U)
#define AW2016_REG_GCR1_CHIPEN_EN (1U << 0U)
#define AW2016_REG_GCR1_CHIPEN_DIS 0U

#define AW2016_REG_ISR 0x02U
#define AW2016_REG_ISR_PUIS (1U << 0U)  // Power-up interrupt status (read clears)

#define AW2016_REG_GCR2 0x04U
#define AW2016_REG_GCR2_IMAX_15MA 0U

#define AW2016_REG_LCTR (0x30)
#define AW2016_REG_LCTR_EXP_LINEAR (1U << 3U)
#define AW2016_REG_LCTR_LE3_EN (1U << 2U)
#define AW2016_REG_LCTR_LE2_EN (1U << 1U)
#define AW2016_REG_LCTR_LE1_EN (1U << 0U)

#define AW2016_REG_LCFG1 0x31U
#define AW2016_REG_LCFG1_CUR_MAX 0x0FU

#define AW2016_REG_LCFG2 0x32U
#define AW2016_REG_LCFG2_CUR_MAX 0x0FU

#define AW2016_REG_LCFG3 0x33U
#define AW2016_REG_LCFG3_CUR_MAX 0x0FU

#define AW2016_REG_PWM1 0x34U

#define AW2016_REG_PWM2 0x35U

#define AW2016_REG_PWM3 0x36U

static uint8_t s_brightness;
static uint32_t s_rgb_current_color = LED_SOFT_WHITE;
static TimerID s_health_check_timer;

// Forward declarations for timer callback
static bool prv_check_and_recover_if_reset(void);

static void prv_health_check_timer_callback(void *data) {
  if (s_brightness == 0U) {
    return;
  }

  if (prv_check_and_recover_if_reset()) {
    PBL_LOG(LOG_LEVEL_WARNING, "AW2016 reset detected, recovered");
    // Reapply current color after recovery
    led_controller_rgb_set_color(s_rgb_current_color);
  }

  // Reschedule while backlight is on
  new_timer_start(s_health_check_timer, AW2016_HEALTH_CHECK_INTERVAL_MS,
                  prv_health_check_timer_callback, NULL, 0);
}

static bool prv_read_register(uint8_t register_address, uint8_t *value) {
  bool ret;

  i2c_use(I2C_AW2016);
  ret = i2c_read_register_block(I2C_AW2016, register_address, 1, value);
  i2c_release(I2C_AW2016);

  return ret;
}

static bool prv_write_register(uint8_t register_address, uint8_t value) {
  bool ret;

  i2c_use(I2C_AW2016);
  ret = i2c_write_register_block(I2C_AW2016, register_address, 1, &value);
  i2c_release(I2C_AW2016);

  return ret;
}

static bool prv_configure_registers(void) {
  bool ret;
  ret = prv_write_register(AW2016_REG_GCR2, AW2016_REG_GCR2_IMAX_15MA);
  ret &= prv_write_register(AW2016_REG_LCTR, AW2016_REG_LCTR_EXP_LINEAR | AW2016_REG_LCTR_LE3_EN |
                                                 AW2016_REG_LCTR_LE2_EN | AW2016_REG_LCTR_LE1_EN);
  ret &= prv_write_register(AW2016_REG_LCFG1, AW2016_REG_LCFG1_CUR_MAX);
  ret &= prv_write_register(AW2016_REG_LCFG2, AW2016_REG_LCFG2_CUR_MAX);
  ret &= prv_write_register(AW2016_REG_LCFG3, AW2016_REG_LCFG3_CUR_MAX);
  return ret;
}

// Check if chip reset (PUIS=1) and recover if needed. Returns true if recovery was performed.
static bool prv_check_and_recover_if_reset(void) {
  uint8_t isr;
  if (!prv_read_register(AW2016_REG_ISR, &isr)) {
    return false;
  }

  if (isr & AW2016_REG_ISR_PUIS) {
    // Chip has reset - re-enable and reconfigure
    prv_write_register(AW2016_REG_GCR1,
                       AW2016_REG_GCR1_CHGDIS_DIS | AW2016_REG_GCR1_CHIPEN_EN);
    prv_configure_registers();
    return true;
  }
  return false;
}

void led_controller_init(void) {
  uint8_t value;
  bool ret;

  ret = prv_read_register(AW2016_REG_RSTR, &value);
  PBL_ASSERTN(ret && (value == AW2016_REG_RSTR_CHIP_ID));

  ret = prv_write_register(AW2016_REG_RSTR, AW2016_REG_RSTR_RST);
  PBL_ASSERTN(ret);

  ret = prv_write_register(AW2016_REG_GCR1, AW2016_REG_GCR1_CHGDIS_DIS);
  PBL_ASSERTN(ret);

  s_health_check_timer = new_timer_create();
}

void led_controller_backlight_set_brightness(uint8_t brightness) {
  bool ret;

  if (brightness > 100U) {
    brightness = 100U;
  }

  // Check if chip reset while we thought it was on - recover if needed
  bool recovered = false;
  if (s_brightness != 0U) {
    recovered = prv_check_and_recover_if_reset();
  }

  if (s_brightness == brightness && !recovered) {
    return;
  }

  const uint8_t previous_brightness = s_brightness;
  s_brightness = brightness;

  if (brightness == 0U) {
    // Stop health check timer when turning off
    new_timer_stop(s_health_check_timer);

    ret = prv_write_register(AW2016_REG_GCR1,
                             AW2016_REG_GCR1_CHGDIS_DIS | AW2016_REG_GCR1_CHIPEN_DIS);
    PBL_ASSERTN(ret);
  } else {
    if (previous_brightness == 0U && !recovered) {
      ret = prv_write_register(AW2016_REG_GCR1,
                               AW2016_REG_GCR1_CHGDIS_DIS | AW2016_REG_GCR1_CHIPEN_EN);
      ret &= prv_configure_registers();
      PBL_ASSERTN(ret);
    }

    led_controller_rgb_set_color(s_rgb_current_color);

    // Verify chip didn't reset during configuration (e.g., from inrush current)
    if (prv_check_and_recover_if_reset()) {
      led_controller_rgb_set_color(s_rgb_current_color);
    }

    // Start health check timer when turning on
    if (previous_brightness == 0U) {
      new_timer_start(s_health_check_timer, AW2016_HEALTH_CHECK_INTERVAL_MS,
                      prv_health_check_timer_callback, NULL, 0);
    }
  }
}

void led_controller_rgb_set_color(uint32_t rgb_color) {
  bool ret;
  uint8_t red, green, blue;

  red = ((rgb_color & 0x00FF0000) >> 16) * s_brightness / 100;
  green = ((rgb_color & 0x0000FF00) >> 8) * s_brightness / 100;
  blue = (rgb_color & 0x000000FF) * s_brightness / 100;

  ret = prv_write_register(AW2016_REG_PWM1, red);
  ret &= prv_write_register(AW2016_REG_PWM2, green);
  ret &= prv_write_register(AW2016_REG_PWM3, blue);

  PBL_ASSERTN(ret);

  s_rgb_current_color = rgb_color;
}

uint32_t led_controller_rgb_get_color(void) {
  return s_rgb_current_color;
}

void command_rgb_set_color(const char *color) {
  uint32_t color_val = strtol(color, NULL, 16);

  led_controller_rgb_set_color(color_val);
}
