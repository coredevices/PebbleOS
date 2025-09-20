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

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/ambient_light.h"
#include "drivers/gpio.h"
#include "drivers/voltage_monitor.h"
#include "drivers/periph_config.h"
#include "kernel/util/sleep.h"
#include "mfg/mfg_info.h"
#include "system/logging.h"
#include "system/passert.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include <inttypes.h>

static uint32_t s_sensor_light_dark_threshold;
static bool s_initialized = false;

static uint32_t prv_get_default_ambient_light_dark_threshold(void) {
  switch (mfg_info_get_watch_color()) {
    // stepped white bezel
    case WATCH_INFO_COLOR_TIME_ROUND_ROSE_GOLD_14:
    case WATCH_INFO_COLOR_TIME_ROUND_SILVER_14:
      return 3200;
    case WATCH_INFO_COLOR_TIME_ROUND_BLACK_14:
    case WATCH_INFO_COLOR_TIME_ROUND_SILVER_20:
      return 3330;
    case WATCH_INFO_COLOR_TIME_ROUND_BLACK_20:
      return 3430;
    default:
      PBL_ASSERTN(BOARD_CONFIG.ambient_light_dark_threshold != 0);
      return BOARD_CONFIG.ambient_light_dark_threshold;
  }
}

//! Turn on or off the ambient light sensor.
//! @param enable True to turn the light sensor on, false to turn it off.
static void prv_sensor_enable(bool enable) {
  const BitAction set = enable ? Bit_SET : Bit_RESET;
  gpio_use(BOARD_CONFIG.photo_en.gpio);
  GPIO_WriteBit(BOARD_CONFIG.photo_en.gpio, BOARD_CONFIG.photo_en.gpio_pin, set);
  gpio_release(BOARD_CONFIG.photo_en.gpio);
}

void ambient_light_init(void) {
  s_sensor_light_dark_threshold = prv_get_default_ambient_light_dark_threshold();

  periph_config_acquire_lock();

  // Initialize light sensor enable
  {
    gpio_use(BOARD_CONFIG.photo_en.gpio);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = BOARD_CONFIG.photo_en.gpio_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(BOARD_CONFIG.photo_en.gpio, &GPIO_InitStructure);

    GPIO_WriteBit(BOARD_CONFIG.photo_en.gpio, BOARD_CONFIG.photo_en.gpio_pin, Bit_RESET);

    gpio_release(BOARD_CONFIG.photo_en.gpio);
  }

  if (BOARD_CONFIG.als_always_on) {
    prv_sensor_enable(true);
  }

  periph_config_release_lock();

  s_initialized = true;
}

uint32_t ambient_light_get_light_level(void) {
  if (!s_initialized) {
    return BOARD_CONFIG.ambient_light_dark_threshold;
  }

  if (!BOARD_CONFIG.als_always_on) {
    prv_sensor_enable(true);
  }

  VoltageReading reading;
  voltage_monitor_read(VOLTAGE_MONITOR_ALS, &reading);

  if (!BOARD_CONFIG.als_always_on) {
    prv_sensor_enable(false);
  }

  // Multiply vmon/vref * 2/3 to find a percentage of the full scale and then
  // multiply it back by 4096 to get a full 12-bit light level.
  uint32_t value = (reading.vmon_total * 4096 * 2) / (reading.vref_total * 3);
  return value;
}

void command_als_read(void) {
  char buffer[16];
  prompt_send_response_fmt(buffer, sizeof(buffer), "%"PRIu32"", ambient_light_get_light_level());
}

uint32_t ambient_light_get_dark_threshold(void) {
  return s_sensor_light_dark_threshold;
}

void ambient_light_set_dark_threshold(uint32_t new_threshold) {
  PBL_ASSERTN(new_threshold <= AMBIENT_LIGHT_LEVEL_MAX);
  s_sensor_light_dark_threshold = new_threshold;
}

bool ambient_light_is_light(void) {
  // if the sensor is not enabled, always return that it is dark
  return s_initialized && ambient_light_get_light_level() > s_sensor_light_dark_threshold;
}

AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  if (!s_initialized) {
    // if the sensor is not enabled, always return that it is very dark
    return AmbientLightLevelUnknown;
  }

  const uint32_t k_delta_threshold = BOARD_CONFIG.ambient_k_delta_threshold;
  if (light_level < (s_sensor_light_dark_threshold - k_delta_threshold)) {
    return AmbientLightLevelVeryDark;
  } else if (light_level < s_sensor_light_dark_threshold) {
    return AmbientLightLevelDark;
  } else if (light_level < (s_sensor_light_dark_threshold + k_delta_threshold)) {
    return AmbientLightLevelLight;
  } else {
    return AmbientLightLevelVeryLight;
  }
}
