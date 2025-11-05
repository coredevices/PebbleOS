/*
 * Copyright 2025 Core Devices LLC
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

#include "drivers/vibe.h"
#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "system/logging.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"

#define AW862XX_REG_SRST                                (0x00)
#define AW862XX_REG_PLAYCFG3                            (0x08)
#define AW862XX_REG_PLAYCFG4                            (0x09)
#define AW862XX_REG_CONTCFG1                            (0x18)
#define AW862XX_REG_CONTCFG2                            (0x19)
#define AW862XX_REG_CONTCFG3                            (0x1A)
#define AW862XX_REG_CONTCFG5                            (0x1C)
#define AW862XX_REG_CONTCFG6                            (0x1D)
#define AW862XX_REG_CONTCFG7                            (0x1E)
#define AW862XX_REG_CONTCFG8                            (0x1F)
#define AW862XX_REG_CONTCFG9                            (0x20)
#define AW862XX_REG_CONTCFG10                           (0x21)
#define AW862XX_REG_CONTCFG11                           (0x22)
#define AW862XX_REG_CONTRD14                            (0x25)
#define AW862XX_REG_CONTRD15                            (0x26)
#define AW862XX_REG_CONTRD16                            (0x27)
#define AW862XX_REG_CONTRD17                            (0x28)
#define AW862XX_REG_SYSCTRL1                            (0x43)
#define AW862XX_REG_SYSCTRL7                            (0x49)
#define AW862XX_REG_CHIPID                              (0x64)

#define AW862XX_BIT_PLAYCFG3_BRK_EN_MASK                (~(1<<2))
#define AW862XX_BIT_PLAYCFG3_BRK                        (1<<2)
#define AW862XX_BIT_PLAYCFG3_BRK_ENABLE                 (1<<2)
#define AW862XX_BIT_PLAYCFG3_BRK_DISABLE                (0<<2)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK             (~(3<<0))
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_STOP             (3<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT             (2<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_RTP              (1<<0)
#define AW862XX_BIT_PLAYCFG3_PLAY_MODE_RAM              (0<<0)

/* PLAYCFG4: reg 0x09 RW */
#define AW862XX_BIT_PLAYCFG4_STOP_MASK                  (~(1<<1))
#define AW862XX_BIT_PLAYCFG4_STOP_ON                    (1<<1)
#define AW862XX_BIT_PLAYCFG4_STOP_OFF                   (0<<1)
#define AW862XX_BIT_PLAYCFG4_GO_MASK                    (~(1<<0))
#define AW862XX_BIT_PLAYCFG4_GO_ON                      (1<<0)
#define AW862XX_BIT_PLAYCFG4_GO_OFF                     (0<<0)

#define AW862XX_CONTCFG1_EDGE_FREQ                      (0xC0)
#define AW862XX_CONTCFG1_WAVE_MODE                      (0x01)  /* 0:sine; 1:cos */
#define AW862XX_CONTCFG1_EN_F0_DET                      (1<<3)
#define AW862XX_CONTCFG2_CONF_F0                        (102)   /* REG = 24,000/f0, f0 is 235Hz */
#define AW862XX_CONTCFG3_DRV_WIDTH                      (209)   /* f0-8-track_margin-brk_gain*/
#define AW862XX_CONTCFG7_FULL_SCALE                     (0x7FL)
#define AW862XX_CONTCFG6_TRACK_MASK                     (~(1<<7))
#define AW862XX_CONTCFG6_TRACK_EN                       (1<<7)
#define AW862XX_SYSCTRL1_VBAT_MODE_MASK                 (~(1<<7))
#define AW862XX_SYSCTRL1_VBAT_MODE_EN                   (1<<7)

#define AW862XX_PWR_OFF_TIME                            (2) /* ms */
#define AW862XX_PWR_ON_TIME                             (8) /* ms */

static bool s_initialized = false;

static bool prv_read_register(uint8_t register_address, uint8_t* data) {
	i2c_use(I2C_AW86225);
	bool rv = i2c_read_register_block(I2C_AW86225, register_address, 1, data);
	i2c_release(I2C_AW86225);
	return rv;
}
  
static bool prv_write_register(uint8_t register_address, uint8_t datum) {
	i2c_use(I2C_AW86225);
	bool rv = i2c_write_register_block(I2C_AW86225, register_address, 1, &datum);
	i2c_release(I2C_AW86225);
	return rv;
}

void prv_modify_reg(uint8_t reg_addr, uint32_t mask, uint8_t reg_data)
{
	uint8_t reg_val = 0;
	uint8_t reg_mask = (uint8_t)mask;

	prv_read_register(reg_addr, &reg_val);
	reg_val &= reg_mask;
	reg_val |= (reg_data & (~reg_mask));
	prv_write_register(reg_addr, reg_val);
}

static void prv_aw862xx_play_go(bool flag)
{
	uint8_t val;

  if (flag) {
		val = AW862XX_BIT_PLAYCFG4_GO_ON;
		prv_write_register(AW862XX_REG_PLAYCFG4, val);
	} else {
		val = AW862XX_BIT_PLAYCFG4_STOP_ON;
		prv_write_register(AW862XX_REG_PLAYCFG4, val);
	}
}


static int prv_f0_detection(void)
{
  int brk_gain = 0x08;
  int track_margain = 0x0F;
  int f0 = 0;
  int f0_pre = 235;
  uint8_t drv_width = 0;
  uint8_t reg_val = 0;

  prv_write_register(0x5A, 0x00);
  prv_modify_reg(0x08, ~(3 << 0), (2 << 0)); // Enter CONT mode
  prv_modify_reg(0x18, ~(1 << 3), (1 << 3)); // Enable F0_DET
  prv_modify_reg(0x1D, ~(1 << 7), (1 << 7)); // Enable track_en
  prv_modify_reg(0x08, ~(1 << 2), (1 << 2)); // Enable auto brake
  prv_modify_reg(0x1D, ~(0x7F << 0), 0x7F);  // Config cont_drv1_lvl
  prv_write_register(0x1E, 0x7F); // Config cont_drv2_lvl
  prv_write_register(0x1F, 0x04); // Config cont_drv1_time
  prv_write_register(0x20, 0x06); // Config cont_drv2_time
  prv_write_register(0x22, 0x0f); // Config cont_track_margin

  drv_width = (uint8_t)(24000 / f0_pre - 8 - brk_gain - track_margain); // Calculate cont_drv_width
  prv_write_register(0x1A, drv_width); // Config cont_drv_width

  prv_write_register(0x09, 0x01); // Start play
  psleep(300);
  prv_write_register(0x09, 0x02); // Stop play

  prv_read_register(0x25, &reg_val); // LRA_F0 high 8 bits
  f0 = reg_val << 8;
  prv_read_register(0x26, &reg_val); // LRA_F0 low 8 bits
  f0 |= reg_val;
  if (f0 == 0) {
    return -1;
  }
  f0 = 384000 / f0;
  
  prv_modify_reg(0x18, ~(1 << 3), (0 << 3)); // Disable F0_DET
  prv_modify_reg(0x08, ~(1 << 2), (0 << 2)); // Close auto brake

  return f0;
}

void vibe_init(void) {
  gpio_output_init(&BOARD_CONFIG_VIBE.ctl, GPIO_OType_PP, GPIO_Speed_2MHz);

  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
  psleep(AW862XX_PWR_OFF_TIME);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, true);
  psleep(AW862XX_PWR_ON_TIME);

  uint8_t chip_id;
  bool ret = prv_read_register(AW862XX_REG_CHIPID, &chip_id);
  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read AW86225 chip ID");
    return;
  }

  ret &= prv_write_register(AW862XX_REG_CONTCFG1, AW862XX_CONTCFG1_EDGE_FREQ  |AW862XX_CONTCFG1_WAVE_MODE | AW862XX_CONTCFG1_EN_F0_DET);
  ret &= prv_write_register(AW862XX_REG_CONTCFG2, AW862XX_CONTCFG2_CONF_F0);
  ret &= prv_write_register(AW862XX_REG_CONTCFG3, AW862XX_CONTCFG3_DRV_WIDTH);
  ret &= prv_write_register(AW862XX_REG_CONTCFG7, AW862XX_CONTCFG7_FULL_SCALE);
  
  prv_modify_reg(AW862XX_REG_CONTCFG6, AW862XX_CONTCFG6_TRACK_MASK, AW862XX_CONTCFG6_TRACK_EN);
  prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_BRK_EN_MASK, AW862XX_BIT_PLAYCFG3_BRK_ENABLE);
  prv_modify_reg(AW862XX_REG_PLAYCFG3, AW862XX_BIT_PLAYCFG3_PLAY_MODE_MASK, AW862XX_BIT_PLAYCFG3_PLAY_MODE_CONT);
  prv_modify_reg(AW862XX_REG_SYSCTRL1, AW862XX_SYSCTRL1_VBAT_MODE_MASK, AW862XX_SYSCTRL1_VBAT_MODE_EN);

  if (!ret) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to initialize AW86225");
    return;
  }

  s_initialized = true;
}

void vibe_set_strength(int8_t strength) {
  if (!s_initialized) {
    return;
  }

  uint32_t scale = strength * AW862XX_CONTCFG7_FULL_SCALE / 100UL;
  prv_write_register(AW862XX_REG_CONTCFG7, (uint8_t)scale);
}

void vibe_ctl(bool on) {
  if (!s_initialized) {
    return;
  }

  if (on) {
    prv_write_register(AW862XX_REG_CONTCFG8, 0xFF);
    prv_write_register(AW862XX_REG_CONTCFG9, 0xFF);
    prv_aw862xx_play_go(true);
  } else {
    prv_aw862xx_play_go(false);
  }

}

void vibe_force_off(void) {
  if (!s_initialized) {
    return;
  }

  prv_aw862xx_play_go(false);
}

int8_t vibe_get_braking_strength(void) {
  if (!s_initialized) {
    return 0;
  }

  uint8_t value;
  prv_read_register(AW862XX_REG_CONTCFG7, &value);
  uint8_t strength = value * 100UL/AW862XX_CONTCFG7_FULL_SCALE;
  return strength;
}

// Refer to DG_AW862XX_Software_Design_Guide_CN_V1.1
status_t vibe_calibrate(void) {
  char f0_cali_lra = 0;
  int f0_cali_step = 0;
  int f0_pre = 235;
  int f0_cali_min = 0;
  int f0_cali_max = 0;
  int f0;
  
  if (!s_initialized) {
    return E_INVALID_OPERATION;
  }

  f0 = prv_f0_detection();
  if (f0 < 0) {
    return E_ERROR;
  }

  /**
   * TRIM_LRA[0:5] is used to calibration the f0 frequency.
   *
   * The TRIM_LRA LSB is 0.24%, up to 31, down to -32, calibration range
   * is nearly +/- 7%.
   *
   * Below code calibrate the f0 to match f0_pre as possible.
   */
  f0_cali_min = f0_pre * 93 / 100;
  f0_cali_max = f0_pre * 107 / 100;
  if (f0 < f0_cali_min || f0 > f0_cali_max) {
    return -1;
  }

  f0_cali_step = 10000 * ((int)f0 - (int)f0_pre) / ((int)f0_pre * 24);
  if (f0_cali_step >= 0) {
    if (f0_cali_step % 10 >= 5) {
      f0_cali_step = 32 + (f0_cali_step / 10 + 1);
    } else {
      f0_cali_step = 32 + f0_cali_step / 10;
    }

  } else {
    if (f0_cali_step % 10 <= -5) {
      f0_cali_step = 32 + (f0_cali_step / 10 - 1);
    } else {
      f0_cali_step = 32 + f0_cali_step / 10;
    }
  }

  if (f0_cali_step > 31) {
    f0_cali_lra = (char)f0_cali_step - 32;
  } else {
    f0_cali_lra = (char)f0_cali_step + 32;
  }

  prv_write_register(0x5A, f0_cali_lra & 0x3F);

  return S_SUCCESS;
}

void command_vibe_ctl(const char *arg) {
  if (!strcmp(arg, "cal")) {
    status_t rc = vibe_calibrate();
    if (rc != S_SUCCESS) {
      prompt_send_response("F0 cali fail");
    } else {
      prompt_send_response("F0 cali success");
    }

    return;
  }

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
