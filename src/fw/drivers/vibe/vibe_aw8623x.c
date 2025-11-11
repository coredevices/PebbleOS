/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/vibe.h"
#include "kernel/util/sleep.h"
#include "system/logging.h"
#include "system/passert.h"

#define AW8623X_PLAYCFG3 0x08U
#define AW8623X_PLAYCFG3_BRK_EN (1U << 2U)
#define AW8623X_PLAYCFG3_PLAY_MODE_CONT (2U << 0U)

#define AW8623X_PLAYCFG4 0x09U
#define AW8623X_PLAYCFG4_STOP (1U << 1U)
#define AW8623X_PLAYCFG4_GO (1U << 0U)

#define AW8623X_CONTCFG1 0x17U
#define AW8623X_CONTCFG1_EDGE_FRE_NONE 0x0U
#define AW8623X_CONTCFG1_SIN_MODE_COS (1U << 4U)
#define AW8623X_CONTCFG1_EN_F0_DET (1U << 5U)

#define AW8623X_CONTCFG2 0x18U
#define AW8623X_CONTCFG2_CONF_F0(freq) (24000U / (freq))

#define AW8623X_CONTCFG3 0x19U
#define AW8623X_CONTCFG3_DRV_WIDTH(freq) (48000U / (freq))

#define AW8623X_CONTCFG6 0x1CU
#define AW8623X_CONTCFG6_DRV1_LVL_MAX 0x7FU
#define AW8623X_CONTCFG6_TRACK_EN (1U << 7U)

#define AW8623X_CONTCFG7 0x1DU
#define AW8623X_CONTCFG7_DRV2_LVL_MAX 0x7FU

#define AW8623X_CONTCFG8 0x1EU
#define AW8623X_CONTCFG8_DRV1_TIME_MAX 0xFFU

#define AW8623X_CONTCFG9 0x1FU
#define AW8623X_CONTCFG9_DRV2_TIME_MAX 0xFFU

#define AW8623X_VBATCTRL 0x4EU
#define AW8623X_VBATCTRL_VBAT_MODE_HW (1U << 6U)

#define AW8623X_IDH 0x57U
#define AW8623X_IDH_CHIPID_H 0x23U

#define AW8623X_IDL 0x69U
#define AW8623X_IDL_CHIPID_L 0x40U

#define AW8623X_PWR_OFF_TIME_MS 2
#define AW8623X_PWR_ON_TIME_MS 3

static bool s_initialized = false;

static bool prv_read_register(uint8_t addr, uint8_t *data) {
  bool ret;

  i2c_use(I2C_AW8623X);
  ret = i2c_read_register_block(I2C_AW8623X, addr, 1, data);
  i2c_release(I2C_AW8623X);

  return ret;
}

static bool prv_write_register(uint8_t addr, uint8_t data) {
  bool ret;

  i2c_use(I2C_AW8623X);
  ret = i2c_write_register_block(I2C_AW8623X, addr, 1, &data);
  i2c_release(I2C_AW8623X);

  return ret;
}

void vibe_init(void) {
  bool ret;
  uint8_t val;

  gpio_output_init(&BOARD_CONFIG_VIBE.ctl, GPIO_OType_PP, GPIO_Speed_2MHz);

  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
  psleep(AW8623X_PWR_OFF_TIME_MS);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, true);
  psleep(AW8623X_PWR_ON_TIME_MS);

  // Verify chip ID
  ret = prv_read_register(AW8623X_IDH, &val);
  if (!ret || val != AW8623X_IDH_CHIPID_H) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read AW8623X chip ID high byte");
    return;
  }

  ret = prv_read_register(AW8623X_IDL, &val);
  if (!ret || val != AW8623X_IDL_CHIPID_L) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read AW8623X chip ID low byte");
    return;
  }

  ret &= prv_write_register(AW8623X_CONTCFG1, AW8623X_CONTCFG1_EDGE_FRE_NONE |
                                                  AW8623X_CONTCFG1_SIN_MODE_COS |
                                                  AW8623X_CONTCFG1_EN_F0_DET);
  ret &= prv_write_register(AW8623X_CONTCFG2, AW8623X_CONTCFG2_CONF_F0(235U));
  ret &= prv_write_register(AW8623X_CONTCFG3, AW8623X_CONTCFG3_DRV_WIDTH(235U));
  ret &= prv_write_register(AW8623X_CONTCFG6,
                            AW8623X_CONTCFG6_DRV1_LVL_MAX | AW8623X_CONTCFG6_TRACK_EN);
  ret &= prv_write_register(AW8623X_CONTCFG7, AW8623X_CONTCFG7_DRV2_LVL_MAX);

  ret &= prv_write_register(AW8623X_PLAYCFG3,
                            AW8623X_PLAYCFG3_BRK_EN | AW8623X_PLAYCFG3_PLAY_MODE_CONT);
  ret &= prv_write_register(AW8623X_VBATCTRL, AW8623X_VBATCTRL_VBAT_MODE_HW);

  PBL_ASSERTN(ret);

  s_initialized = true;
}

void vibe_set_strength(int8_t strength) {
  bool ret;
  uint8_t scale;

  if (!s_initialized) {
    return;
  }

  if (strength < 0) {
    strength = -strength;
  }

  scale = ((uint16_t)strength * AW8623X_CONTCFG7_DRV2_LVL_MAX) / 100U;

  ret &= prv_write_register(AW8623X_CONTCFG6, scale | AW8623X_CONTCFG6_TRACK_EN);
  ret = prv_write_register(AW8623X_CONTCFG7, scale);
  PBL_ASSERTN(ret);
}

void vibe_ctl(bool on) {
  bool ret;

  if (!s_initialized) {
    return;
  }

  if (on) {
    ret = prv_write_register(AW8623X_CONTCFG8, AW8623X_CONTCFG8_DRV1_TIME_MAX);
    ret &= prv_write_register(AW8623X_CONTCFG9, AW8623X_CONTCFG9_DRV2_TIME_MAX);
    ret &= prv_write_register(AW8623X_PLAYCFG4, AW8623X_PLAYCFG4_GO);
  } else {
    ret = prv_write_register(AW8623X_PLAYCFG4, AW8623X_PLAYCFG4_STOP);
  }

  PBL_ASSERTN(ret);
}

void vibe_force_off(void) {
  vibe_ctl(false);
}

int8_t vibe_get_braking_strength(void) {
  bool ret;
  uint8_t val;

  if (!s_initialized) {
    return 0;
  }

  ret = prv_read_register(AW8623X_CONTCFG7, &val);
  PBL_ASSERTN(ret);

  return ((int16_t)val * 100) / AW8623X_CONTCFG7_DRV2_LVL_MAX;
}

status_t vibe_calibrate(void) {
  return E_INVALID_OPERATION;
}

void command_vibe_ctl(const char *arg) {
  int8_t strength;

  if (strcmp(arg, "cal") == 0) {
    status_t ret = vibe_calibrate();
    if (ret != S_SUCCESS) {
      prompt_send_response("Calibration failed");
    } else {
      prompt_send_response("Calibration succeeded");
    }

    return;
  }

  strength = (int8_t)atoi(arg);
  if ((strength < VIBE_STRENGTH_MIN) || (strength > VIBE_STRENGTH_MAX)) {
    prompt_send_response("Invalid argument");
    return;
  }

  vibe_set_strength(strength);
  vibe_ctl(strength != 0);

  prompt_send_response("OK");
}
