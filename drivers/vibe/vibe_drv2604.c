/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/vibe.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>

#include <board/board.h>

PBL_LOG_MODULE_DEFINE(driver_vibe_drv2604, CONFIG_DRIVER_VIBE_LOG_LEVEL);

/* XXX: tune RATED_VOLTAGE? / OD_CLAMP? */

#define DRV2604_STATUS                      0x00
#define DRV2604_MODE                        0x01
#define DRV2604_MODE_MODE_MASK              PBL_GENMASK(2, 0)
#define DRV2604_MODE_TRIGGER                PBL_FIELD_PREP(DRV2604_MODE_MODE_MASK, 0x0)
#define DRV2604_MODE_RTP                    PBL_FIELD_PREP(DRV2604_MODE_MODE_MASK, 0x5)
#define DRV2604_MODE_AUTOCAL                PBL_FIELD_PREP(DRV2604_MODE_MODE_MASK, 0x7)
#define DRV2604_MODE_STANDBY                PBL_BIT(6)
#define DRV2604_RTP_INPUT                   0x02
#define DRV2604_GO                          0x0C
#define DRV2604_GO_GO                       PBL_BIT(0)
#define DRV2604_RATED_VOLTAGE               0x16
#define DRV2604_OD_CLAMP                    0x17
#define DRV2604_A_CAL_COMP                  0x18
#define DRV2604_A_CAL_BEMF                  0x19
#define DRV2604_FBCTL                       0x1A
#define DRV2604_FBCTL_LRA                   PBL_BIT(7)
#define DRV2604_FBCTL_FB_BRAKE_FACTOR_MASK  PBL_GENMASK(6, 4)
#define DRV2604_FBCTL_FB_BRAKE_FACTOR(n)    PBL_FIELD_PREP(DRV2604_FBCTL_FB_BRAKE_FACTOR_MASK, (n))
#define DRV2604_FBCTL_LOOP_GAIN_MASK        PBL_GENMASK(3, 2)
#define DRV2604_FBCTL_LOOP_GAIN(n)          PBL_FIELD_PREP(DRV2604_FBCTL_LOOP_GAIN_MASK, (n))
#define DRV2604_FBCTL_BEMF_GAIN_MASK        PBL_GENMASK(1, 0)
#define DRV2604_FBCTL_BEMF_GAIN(n)          PBL_FIELD_PREP(DRV2604_FBCTL_BEMF_GAIN_MASK, (n))
#define DRV2604_CONTROL1                    0x1B
#define DRV2604_CONTROL1_STARTUP_BOOST      PBL_BIT(7)
#define DRV2604_CONTROL1_DRIVE_TIME_MASK    PBL_GENMASK(4, 0)
#define DRV2604_CONTROL1_DRIVE_TIME(n)      PBL_FIELD_PREP(DRV2604_CONTROL1_DRIVE_TIME_MASK, (n))
#define DRV2604_CONTROL2                    0x1C
#define DRV2604_CONTROL2_BIDIR_INPUT        PBL_BIT(7)
#define DRV2604_CONTROL2_BRAKE_STABILIZER   PBL_BIT(6)
#define DRV2604_CONTROL2_SAMPLE_TIME_MASK   PBL_GENMASK(5, 4)
#define DRV2604_CONTROL2_SAMPLE_TIME(n)     PBL_FIELD_PREP(DRV2604_CONTROL2_SAMPLE_TIME_MASK, (n))
#define DRV2604_CONTROL2_BLANKING_TIME_MASK PBL_GENMASK(3, 2)
#define DRV2604_CONTROL2_BLANKING_TIME(n)   PBL_FIELD_PREP(DRV2604_CONTROL2_BLANKING_TIME_MASK, (n))
#define DRV2604_CONTROL2_IDISS_TIME_MASK    PBL_GENMASK(1, 0)
#define DRV2604_CONTROL2_IDISS_TIME(n)      PBL_FIELD_PREP(DRV2604_CONTROL2_IDISS_TIME_MASK, (n))
#define DRV2604_CONTROL4                    0x1E
#define DRV2604_CONTROL4_AUTO_CAL_TIME_MASK PBL_GENMASK(5, 4)
#define DRV2604_CONTROL4_AUTO_CAL_TIME(n)   PBL_FIELD_PREP(DRV2604_CONTROL4_AUTO_CAL_TIME_MASK, (n))

static bool s_initialized = false;

static bool prv_read_register(uint8_t register_address, uint8_t *result) {
  i2c_use(I2C_DRV2604);
  bool rv = i2c_read_register(I2C_DRV2604, register_address, result);
  i2c_release(I2C_DRV2604);
  return rv;
}

static bool prv_write_register(uint8_t register_address, uint8_t datum) {
  i2c_use(I2C_DRV2604);
  uint8_t block[2] = {register_address, datum};
  bool rv = i2c_write_block(I2C_DRV2604, 2, block);
  i2c_release(I2C_DRV2604);
  return rv;
}

void vibe_init(void) {
  gpio_output_init(&BOARD_CONFIG_VIBE.ctl, GPIO_OType_PP);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, true);
  uint8_t rv;
  bool found = prv_read_register(DRV2604_STATUS, &rv);
  if (!found) {
    PBL_LOG_ERR("Failed to read the STATUS register");
    return;
  }

  /* calibration table maybe should live in the board file? */
  const uint8_t regs[][2] = {
    {DRV2604_MODE, DRV2604_MODE_TRIGGER},
    {DRV2604_FBCTL, DRV2604_FBCTL_LRA | DRV2604_FBCTL_FB_BRAKE_FACTOR(2) |
                        DRV2604_FBCTL_LOOP_GAIN(2) | DRV2604_FBCTL_BEMF_GAIN(2)},
    {DRV2604_RATED_VOLTAGE, 0x3F /* default */},
    {DRV2604_OD_CLAMP, 0x89 /* default */},
    {DRV2604_A_CAL_COMP, 0x0D},
    {DRV2604_A_CAL_BEMF, 0x80},
    {DRV2604_CONTROL1,
     DRV2604_CONTROL1_STARTUP_BOOST | DRV2604_CONTROL1_DRIVE_TIME(0x10 /* 2.1 ms */)},
    {DRV2604_CONTROL2, DRV2604_CONTROL2_BIDIR_INPUT | DRV2604_CONTROL2_BRAKE_STABILIZER |
                           DRV2604_CONTROL2_SAMPLE_TIME(3) | DRV2604_CONTROL2_BLANKING_TIME(1) |
                           DRV2604_CONTROL2_IDISS_TIME(1)},
    {DRV2604_MODE, DRV2604_MODE_STANDBY | DRV2604_MODE_TRIGGER},
  };

  for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
    if (!prv_write_register(regs[i][0], regs[i][1])) {
      PBL_LOG_ERR("failed to write register %02x on DRV2604", regs[i][0]);
      gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
      return;
    }
  }

  // DRV2604 does not get its registers reset by disabling EN, so it's ok to
  // do that
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);

  s_initialized = true;
}

static bool s_vibe_ctl_on = false;

/* Sadly, you cannot play music with DRV2604 this way.  Maybe we should
 * modulate DRIVE_TIME too?
 */
void vibe_set_strength(int8_t strength) {
  int32_t strength_scale = strength * 0x7FL / 100L;
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, true);
  prv_write_register(DRV2604_MODE, DRV2604_MODE_RTP); /* exit standby, RTP mode */
  prv_write_register(DRV2604_RTP_INPUT, strength_scale);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, s_vibe_ctl_on);
}

void vibe_ctl(bool on) {
  if (!s_initialized) {
    return;
  }

  PBL_LOG_DBG("Vibe status <%s>", on ? "on" : "off");

  if (!on) {
    prv_write_register(
        DRV2604_MODE,
        DRV2604_MODE_STANDBY |
            DRV2604_MODE_RTP); /* enter standby even if the enable GPIO is not hooked up */
  }
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, on);
  s_vibe_ctl_on = on;
  if (on) {
    prv_write_register(DRV2604_MODE, DRV2604_MODE_RTP); /* exit standby, RTP mode */
  }
}

void vibe_force_off(void) {
  if (!s_initialized) {
    return;
  }
  prv_write_register(
      DRV2604_MODE,
      DRV2604_MODE_STANDBY |
          DRV2604_MODE_RTP); /* enter standby even if the enable GPIO is not hooked up */
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
  s_vibe_ctl_on = false;
}

int8_t vibe_get_braking_strength(void) {
  // We support the -100..100 range because BIDIR_INPUT is set
  return VIBE_STRENGTH_MIN;
}

status_t vibe_calibrate(void) {
  bool bad = false;
  bad |= !prv_write_register(DRV2604_MODE, DRV2604_MODE_AUTOCAL);
  bad |= !prv_write_register(DRV2604_FBCTL, DRV2604_FBCTL_LRA | DRV2604_FBCTL_FB_BRAKE_FACTOR(2) |
                                                DRV2604_FBCTL_LOOP_GAIN(2));
  bad |= !prv_write_register(DRV2604_RATED_VOLTAGE, 0x3F); /* default value */
  bad |= !prv_write_register(DRV2604_OD_CLAMP, 0x89);      /* default value */
  bad |= !prv_write_register(DRV2604_CONTROL1, DRV2604_CONTROL1_STARTUP_BOOST |
                                                   DRV2604_CONTROL1_DRIVE_TIME(0x10 /* 2.1 ms */));
  bad |= !prv_write_register(
      DRV2604_CONTROL2, DRV2604_CONTROL2_BIDIR_INPUT | DRV2604_CONTROL2_BRAKE_STABILIZER |
                            DRV2604_CONTROL2_SAMPLE_TIME(3) | DRV2604_CONTROL2_BLANKING_TIME(1) |
                            DRV2604_CONTROL2_IDISS_TIME(1));
  bad |= !prv_write_register(DRV2604_CONTROL4, DRV2604_CONTROL4_AUTO_CAL_TIME(3));
  bad |= !prv_write_register(DRV2604_GO, DRV2604_GO_GO);

  return bad ? E_ERROR : S_SUCCESS;
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
    pbl_shell_print(sh, "vibe cal...");
    if (vibe_calibrate() != S_SUCCESS) {
      pbl_shell_error(sh, "vibe cal failed");
      return -EIO;
    }
    pbl_shell_print(sh, "vibe cal succeeded");
    return 0;
  }

  if (strcmp(argv[1], "reg") == 0) {
    pbl_shell_print(sh, "vibe regs:");
    for (int i = 0; i <= 0x22; i++) {
      uint8_t reg;
      prv_read_register(i, &reg);
      pbl_shell_print(sh, "  vibe reg %02x: %02x", i, reg);
    }
    return 0;
  }

  long strength;
  if (pbl_shell_strtol(argv[1], &strength) != 0 || strength < 0 || strength > VIBE_STRENGTH_MAX) {
    pbl_shell_error(sh, "invalid argument '%s'", argv[1]);
    return -EINVAL;
  }

  vibe_set_strength((int8_t)strength);
  vibe_ctl(strength != 0);
  return 0;
}

PBL_SHELL_CMD_ARG_REGISTER(vibe, nullptr,
                           "Vibrate at <strength 0-100>, <cal> to calibrate or <reg> to dump",
                           prv_cmd_vibe, 2, 0);
#endif
