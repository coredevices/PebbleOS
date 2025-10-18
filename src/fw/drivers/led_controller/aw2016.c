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

#include "drivers/led_controller.h"

#include "board/board.h"
#include "drivers/i2c.h"
#include "system/logging.h"
#include "system/passert.h"

#define AW2016_REG_CHIP_ID              (0x00)
#define AW2016_REG_GCR1                 (0x01)
#define AW2016_REG_GCR1__CHGDIS         (1<<1)
#define AW2016_REG_GCR1__ENABLE         (1)
#define AW2016_REG_GCR1__DISABLE        (0)
#define AW2016_REG_GCR2                 (0x04)
#define AW2016_REG_GCR2__IMAX_30MA      (1)
#define AW2016_REG_LCTR                 (0x30)
#define AW2016_REG_LCTR__LINEAR         (1<<3)
#define AW2016_REG_LCTR__EN3            (1<<2)
#define AW2016_REG_LCTR__EN2            (1<<1)
#define AW2016_REG_LCTR__EN1            (1<<0)
#define AW2016_REG_LCFG1                (0x31)
#define AW2016_REG_LCFG1__CUR           (0x0F)
#define AW2016_REG_LCFG2                (0x32)
#define AW2016_REG_LCFG2__CUR           (0x0F)
#define AW2016_REG_LCFG3                (0x33)
#define AW2016_REG_LCFG3__CUR           (0x0F)
#define AW2016_REG_DIM1                 (0x34)
#define AW2016_REG_DIM2                 (0x35)
#define AW2016_REG_DIM3                 (0x36)

static uint8_t s_brightness;
static uint32_t s_rgb_current_color = LED_WARM_WHITE;

static bool prv_read_register(uint8_t register_address, uint8_t* value) {
  i2c_use(I2C_AW2016);
	bool rv = i2c_read_register_block(I2C_AW2016, register_address, 1, value);
	i2c_release(I2C_AW2016);
	return rv;
}

static bool prv_write_register(uint8_t register_address, uint8_t value) {
  i2c_use(I2C_AW2016);
	bool rv = i2c_write_register_block(I2C_AW2016, register_address, 1, &value);
	i2c_release(I2C_AW2016);
	return rv;
}

static void prv_aw2016_disable(void) {
  prv_write_register(AW2016_REG_GCR1, AW2016_REG_GCR1__CHGDIS|AW2016_REG_GCR1__DISABLE);
}

void led_controller_init(void) {
  uint8_t value;
  bool rv = prv_read_register(AW2016_REG_CHIP_ID, &value);
  PBL_ASSERT(rv, "Failed to get AW2016 chip ID");

  //intialize regs
  rv &= prv_write_register(AW2016_REG_GCR1, AW2016_REG_GCR1__CHGDIS|AW2016_REG_GCR1__ENABLE);
  rv &= prv_write_register(AW2016_REG_GCR2, AW2016_REG_GCR2__IMAX_30MA);
  rv &= prv_write_register(AW2016_REG_LCTR, AW2016_REG_LCTR__LINEAR|AW2016_REG_LCTR__EN3|AW2016_REG_LCTR__EN2|AW2016_REG_LCTR__EN1);
  rv &= prv_write_register(AW2016_REG_LCFG1, AW2016_REG_LCFG1__CUR);
  rv &= prv_write_register(AW2016_REG_LCFG2, AW2016_REG_LCFG2__CUR);
  rv &= prv_write_register(AW2016_REG_LCFG3, AW2016_REG_LCFG3__CUR);

  PBL_ASSERT(rv, "Failed to initialize AW2016");
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
    prv_aw2016_disable();
  } else {
    led_controller_rgb_set_color(s_rgb_current_color);
  }
}


void led_controller_rgb_set_color(uint32_t rgb_color) {
  s_rgb_current_color = rgb_color;

  uint8_t red = (s_rgb_current_color & 0x00FF0000) >> 16;
  red = red * s_brightness / 100;
  uint8_t green = (s_rgb_current_color & 0x0000FF00) >> 8;
  green = green * s_brightness / 100;
  uint8_t blue = (s_rgb_current_color & 0x000000FF);
  blue = blue * s_brightness / 100;

  bool rv = prv_write_register(AW2016_REG_GCR1, AW2016_REG_GCR1__CHGDIS|AW2016_REG_GCR1__ENABLE);
  rv &= prv_write_register(AW2016_REG_DIM1, red);
  rv &= prv_write_register(AW2016_REG_DIM2, green);
  rv &= prv_write_register(AW2016_REG_DIM3, blue);

  if(!rv) {
    PBL_LOG(LOG_LEVEL_ERROR, "aw2016 change color failed");
  }
}

uint32_t led_controller_rgb_get_color(void) {
  return s_rgb_current_color;
}

void command_rgb_set_color(const char* color) {
  uint32_t color_val = strtol(color, NULL, 16);

  led_controller_rgb_set_color(color_val);
}

