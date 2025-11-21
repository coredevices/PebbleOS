/*
 * Copyright 2025 Core Devices
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
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "kernel/util/sleep.h"
#include "mfg/mfg_info.h"
#include "system/logging.h"
#include "system/passert.h"

#include <inttypes.h>

// Registers
#define W1160_STATE_REG             0x00
#define W1160_IT_FAST1_REG          0x02
#define W1160_IT_FAST2_REG          0x03
#define W1160_SAMPLE_FAST1_REG      0x05
#define W1160_SAMPLE_FAST2_REG      0x06
#define W1160_SAMPLE_SLOW1_REG      0x07
#define W1160_SAMPLE_SLOW2_REG      0x08
#define W1160_FLAG1_REG             0x10
#define W1160_DATA1_ALS_REG         0x13
#define W1160_DATA2_ALS_REG         0x14
#define W1160_DATA_GC_REG           0x17
#define W1160_POWER_MODE_REG        0x3D
#define W1160_PDT_ID_REG            0x3E
#define W1160_FIFOCTRL1_REG         0x60
#define W1160_FIFOCTRL1_REG_VALUE   0x20
#define W1160_FIFO1_WM_LV_REG       0x61
#define W1160_FIFO2_WM_LV_REG       0x62
#define W1160_FIFOCTRL2_REG         0x63
#define W1160_FIFO_FCNT1_REG        0x64
#define W1160_FIFO_FCNT2_REG        0x65
#define W1160_FIFO_OUT_REG          0x66
#define W1160_FIFO_FLAG_REG         0x67
#define W1160_FIFOCTRL3_REG         0x68
#define W1160_FIFOCTRL4_REG         0x69
#define W1160_AGCCTRL1_REG          0x6A
#define W1160_MANUAL_GAIN_CTRL_REG  0x6B
#define W1160_AGCCTRL2_REG          0x6C
#define W1160_THD_SAT_GC_REG        0x6D
#define W1160_IT_SLOW1_REG          0x6E
#define W1160_IT_SLOW2_REG          0x6F
#define W1160_SOFT_RESET_REG        0x80
// Configuration register bits
#define W1160_ALSCTRL_REG           0xA4

#define W1160_POR_WAIT_TIME         (10)     /* ms */
#define W1160_AGCCTRL1_AGC_EN       (0<<7)   /* 1:en; 0:dis */
#define W1160_AGCCTRL1_MGC_EN       (1)      /* 1:en; 0:dis */
#define W1160_AGCCTRL2_SEL_MODE     (1<<2)   /* must be 1 */
#define W1160_AGCCTRL2_12B_MODE     (0<<3)   /* 1:12bit */
#define W1160_ALSCTRL_SAT_EN        (0<<1)   /* 1:en; 0:dis */
#define W1160_ALSCTRL_DATA_FORMAT12 (0<<2)   /* 1:12bit 0:16bit */
#define W1160_DATA_GC_LVL           (15<<4)  /* gc lelvel 15 */
#define W1160_SAT_GC_CONFIG         (0x0A)
#define W1160_SLOW_IT_CONFIG1       (0x00)   /* 16.830ms */
#define W1160_SLOW_IT_CONFIG2       (0xA9)
#define W1160_SLOW_ST_CONFIG1       (0x03)   /* 480ms */
#define W1160_SLOW_ST_CONFIG2       (0xFF)
#define W1160_SAMPLING_EN           (1<<1)   /* 1:en 0:dis */
#define W1160_CHIP_ID               (0xE5)

#define W1160_RESULT_EXPONENT_SHIFT (12)
#define W1160_RESULT_MANTISSA_MASK  (0x0FFF)
#define W1160_ADC2LUX_COEF          (3U)

static bool s_initialized;
static uint32_t s_sensor_light_dark_threshold;

static bool prv_read_register(uint8_t register_address, uint8_t *result) {
  i2c_use(I2C_W1160);
  bool rv = i2c_read_register_block(I2C_W1160, register_address, 1, result);
  i2c_release(I2C_W1160);
  return rv;
}

static bool prv_write_register(uint8_t register_address, uint8_t datum) {
  i2c_use(I2C_W1160);
  bool rv = i2c_write_register_block(I2C_W1160, register_address, 1, &datum);
  i2c_release(I2C_W1160);
  return rv;
}

void ambient_light_init(void) {
  uint8_t chip_id;
  bool rv;

  s_sensor_light_dark_threshold = BOARD_CONFIG.ambient_light_dark_threshold;

  psleep(W1160_POR_WAIT_TIME);

  rv = prv_read_register(W1160_PDT_ID_REG, &chip_id);
  if (!rv) {
    PBL_LOG(LOG_LEVEL_ERROR, "Could not read W1160 chip ID");
    return;
  }

  if (chip_id != W1160_CHIP_ID) {
    PBL_LOG(LOG_LEVEL_ERROR, "Unexpected W1160 chip ID: 0x%02x", chip_id);
    return;
  }

  rv = prv_write_register(W1160_AGCCTRL1_REG, W1160_AGCCTRL1_AGC_EN);
  rv &= prv_write_register(W1160_MANUAL_GAIN_CTRL_REG, W1160_AGCCTRL1_MGC_EN);
  rv &= prv_write_register(W1160_DATA_GC_REG, W1160_DATA_GC_LVL);
  rv &= prv_write_register(W1160_AGCCTRL2_REG, W1160_AGCCTRL2_SEL_MODE|W1160_AGCCTRL2_12B_MODE);
  rv &= prv_write_register(W1160_ALSCTRL_REG, W1160_ALSCTRL_SAT_EN|W1160_ALSCTRL_DATA_FORMAT12);
  rv &= prv_write_register(W1160_THD_SAT_GC_REG, W1160_SAT_GC_CONFIG);
  rv &= prv_write_register(W1160_IT_SLOW1_REG, W1160_SLOW_IT_CONFIG1);
  rv &= prv_write_register(W1160_IT_SLOW2_REG, W1160_SLOW_IT_CONFIG2);
  rv &= prv_write_register(W1160_SAMPLE_SLOW1_REG, W1160_SLOW_ST_CONFIG1);
  rv &= prv_write_register(W1160_SAMPLE_SLOW2_REG, W1160_SLOW_ST_CONFIG2);
  rv &= prv_write_register(W1160_STATE_REG, W1160_SAMPLING_EN);

  PBL_ASSERT(rv, "Failed to initialize W1160");

  s_initialized = true;
}

uint32_t ambient_light_get_light_level(void) {
  uint8_t result[2] = {0};
  bool rv;

  if (!s_initialized) {
    return 0UL;
  }

  rv = prv_read_register(W1160_DATA1_ALS_REG, &result[1]);
  rv &= prv_read_register(W1160_DATA2_ALS_REG, &result[0]);
  PBL_ASSERT(rv, "Failed to read als data");

  uint16_t als = (((uint16_t)(result[1])) << 8) | result[0];

  return als;
}

void command_als_read(void) {
  char buffer[16] = {0};
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
  return ambient_light_get_light_level() > s_sensor_light_dark_threshold;
}

AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
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
