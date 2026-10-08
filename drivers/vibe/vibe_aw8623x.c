/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <board/board.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe.h>
#include <kernel/util/sleep.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <system/passert.h>

PBL_LOG_MODULE_DEFINE(driver_vibe_aw8623x, CONFIG_DRIVER_VIBE_LOG_LEVEL);

#define AW8623X_PLAYCFG3                0x08U
#define AW8623X_PLAYCFG3_BRK_EN         PBL_BIT(2)
#define AW8623X_PLAYCFG3_PLAY_MODE_MASK PBL_GENMASK(1, 0)
#define AW8623X_PLAYCFG3_PLAY_MODE_CONT 0x2U

#define AW8623X_PLAYCFG4      0x09U
#define AW8623X_PLAYCFG4_STOP PBL_BIT(1)
#define AW8623X_PLAYCFG4_GO   PBL_BIT(0)

#define AW8623X_CONTCFG1               0x17U
#define AW8623X_CONTCFG1_EDGE_FRE_NONE 0x0U
#define AW8623X_CONTCFG1_SIN_MODE_COS  PBL_BIT(4)

#define AW8623X_CONTCFG2               0x18U
#define AW8623X_CONTCFG2_CONF_F0(freq) (24000U / (freq))

#define AW8623X_CONTCFG3                 0x19U
#define AW8623X_CONTCFG3_DRV_WIDTH(freq) (48000U / (freq))

#define AW8623X_CONTCFG6               0x1CU
#define AW8623X_CONTCFG6_TRACK_EN      PBL_BIT(7)
#define AW8623X_CONTCFG6_DRV1_LVL_MASK PBL_GENMASK(6, 0)

#define AW8623X_CONTCFG7               0x1DU
#define AW8623X_CONTCFG7_DRV2_LVL_MASK PBL_GENMASK(6, 0)
#define AW8623X_CONTCFG7_DRV2_LVL_MAX  0x7FU

#define AW8623X_CONTCFG8               0x1EU
#define AW8623X_CONTCFG8_DRV1_TIME_MAX 0xFFU

#define AW8623X_CONTCFG9               0x1FU
#define AW8623X_CONTCFG9_DRV2_TIME_MAX 0xFFU

#define AW8623X_GLBRD5               0x3FU
#define AW8623X_GLBRD5_STATE_MASK    PBL_GENMASK(3, 0)
#define AW8623X_GLBRD5_STATE_STANDBY 0x00U

#define AW8623X_SYSCTRL2         0x46U
#define AW8623X_SYSCTRL2_STANDBY PBL_BIT(6)

#define AW8623X_VBATCTRL              0x4EU
#define AW8623X_VBATCTRL_VBAT_MODE_HW PBL_BIT(6)

#define AW8623X_IDH          0x57U
#define AW8623X_IDH_CHIPID_H 0x23U

#define AW8623X_IDL          0x69U
#define AW8623X_IDL_CHIPID_L 0x40U

#define AW8623X_PWR_OFF_TIME_MS 2
#define AW8623X_PWR_ON_TIME_MS  3
#define AW8623X_STOP_RETRIES    40
#define AW8623X_STOP_POLL_MS    2

static bool s_initialized = false;
static int8_t s_target_strength = VIBE_STRENGTH_MAX;

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

static bool prv_set_standby(bool standby) {
  uint8_t val;
  if (!prv_read_register(AW8623X_SYSCTRL2, &val)) {
    return false;
  }
  val = (val & ~AW8623X_SYSCTRL2_STANDBY) | PBL_FIELD_PREP(AW8623X_SYSCTRL2_STANDBY, standby);
  return prv_write_register(AW8623X_SYSCTRL2, val);
}

static bool prv_wait_for_standby(void) {
  for (unsigned int i = 0; i < AW8623X_STOP_RETRIES; i++) {
    uint8_t val;
    if (!prv_read_register(AW8623X_GLBRD5, &val)) {
      return false;
    }
    if (PBL_FIELD_GET(AW8623X_GLBRD5_STATE_MASK, val) == AW8623X_GLBRD5_STATE_STANDBY) {
      return true;
    }
    psleep(AW8623X_STOP_POLL_MS);
  }
  return false;
}

static void prv_stop(void) {
  if (prv_write_register(AW8623X_PLAYCFG4, AW8623X_PLAYCFG4_STOP) && prv_wait_for_standby()) {
    return;
  }
  PBL_LOG_WRN("AW8623X: playback stop failed, forcing standby");
  if (prv_set_standby(true) && prv_wait_for_standby()) {
    return;
  }
  PBL_LOG_ERR("AW8623X: failed to confirm playback stop, resetting driver");
  vibe_init();
}

void vibe_init(void) {
  bool ret;
  uint8_t val;

  s_initialized = false;
  gpio_output_init(&BOARD_CONFIG_VIBE.ctl, GPIO_OType_PP);

  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, true);
  psleep(AW8623X_PWR_OFF_TIME_MS);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
  psleep(AW8623X_PWR_ON_TIME_MS);

  // Verify chip ID
  ret = prv_read_register(AW8623X_IDH, &val);
  if (!ret || val != AW8623X_IDH_CHIPID_H) {
    PBL_LOG_ERR("Failed to read AW8623X chip ID high byte");
    return;
  }

  ret = prv_read_register(AW8623X_IDL, &val);
  if (!ret || val != AW8623X_IDL_CHIPID_L) {
    PBL_LOG_ERR("Failed to read AW8623X chip ID low byte");
    return;
  }

  // F0 detection disables automatic braking; normal playback only needs tracking.
  ret &= prv_write_register(AW8623X_CONTCFG1,
                            AW8623X_CONTCFG1_EDGE_FRE_NONE | AW8623X_CONTCFG1_SIN_MODE_COS);
  ret &= prv_write_register(AW8623X_CONTCFG2, AW8623X_CONTCFG2_CONF_F0(235U));
  ret &= prv_write_register(AW8623X_CONTCFG3, AW8623X_CONTCFG3_DRV_WIDTH(235U));
  const uint8_t scale = ((uint16_t)s_target_strength * AW8623X_CONTCFG7_DRV2_LVL_MAX) / 100U;
  ret &=
      prv_write_register(AW8623X_CONTCFG6, PBL_FIELD_PREP(AW8623X_CONTCFG6_DRV1_LVL_MASK, scale) |
                                               AW8623X_CONTCFG6_TRACK_EN);
  ret &=
      prv_write_register(AW8623X_CONTCFG7, PBL_FIELD_PREP(AW8623X_CONTCFG7_DRV2_LVL_MASK, scale));

  ret &= prv_write_register(
      AW8623X_PLAYCFG3, AW8623X_PLAYCFG3_BRK_EN | PBL_FIELD_PREP(AW8623X_PLAYCFG3_PLAY_MODE_MASK,
                                                                 AW8623X_PLAYCFG3_PLAY_MODE_CONT));
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

  s_target_strength = strength;
  scale = ((uint16_t)strength * AW8623X_CONTCFG7_DRV2_LVL_MAX) / 100U;

  ret = prv_write_register(AW8623X_CONTCFG6, PBL_FIELD_PREP(AW8623X_CONTCFG6_DRV1_LVL_MASK, scale) |
                                                 AW8623X_CONTCFG6_TRACK_EN);
  ret &=
      prv_write_register(AW8623X_CONTCFG7, PBL_FIELD_PREP(AW8623X_CONTCFG7_DRV2_LVL_MASK, scale));
  PBL_ASSERTN(ret);
}

void vibe_ctl(bool on) {
  bool ret;

  if (!s_initialized) {
    return;
  }

  if (on) {
    // A failed STOP may have left forced standby enabled.
    ret = prv_set_standby(false);
    ret &= prv_write_register(AW8623X_CONTCFG8, AW8623X_CONTCFG8_DRV1_TIME_MAX);
    ret &= prv_write_register(AW8623X_CONTCFG9, AW8623X_CONTCFG9_DRV2_TIME_MAX);
    ret &= prv_write_register(AW8623X_PLAYCFG4, AW8623X_PLAYCFG4_GO);
  } else {
    prv_stop();
    return;
  }

  PBL_ASSERTN(ret);
}

void vibe_force_off(void) {
  vibe_ctl(false);
}

int8_t vibe_get_braking_strength(void) {
  return VIBE_STRENGTH_OFF;
}

status_t vibe_calibrate(void) {
  return E_INVALID_OPERATION;
}

uint8_t vibe_get_calibration(void) {
  return 0xFF;
}

void vibe_apply_calibration(uint8_t cali) {
}

#ifdef CONFIG_SHELL
#include <errno.h>
#include <string.h>

#include <pbl/shell/shell.h>

static int prv_cmd_vibe(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (strcmp(argv[1], "cal") == 0) {
    if (vibe_calibrate() != S_SUCCESS) {
      pbl_shell_error(sh, "calibration failed");
      return -EIO;
    }
    pbl_shell_print(sh, "calibration succeeded");
    return 0;
  }

  long strength;
  if (pbl_shell_strtol(argv[1], &strength) != 0 || strength < VIBE_STRENGTH_MIN ||
      strength > VIBE_STRENGTH_MAX) {
    pbl_shell_error(sh, "invalid argument '%s'", argv[1]);
    return -EINVAL;
  }

  vibe_set_strength((int8_t)strength);
  vibe_ctl(strength != 0);
  return 0;
}

PBL_SHELL_CMD_ARG_REGISTER(vibe, NULL, "Vibrate at <strength -100-100>, or <cal> to calibrate",
                           prv_cmd_vibe, 2, 0);
#endif
