/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/vibe.h>
#include <pbl/shell/shell.h>

static int prv_cmd_start(const struct pbl_shell *sh, size_t argc, char **argv) {
  long strength;
  int ret;

  if (pbl_shell_strtol(argv[1], &strength) != 0 || strength < VIBE_STRENGTH_MIN ||
      strength > VIBE_STRENGTH_MAX) {
    pbl_shell_error(sh, "invalid strength '%s'", argv[1]);
    return -EINVAL;
  }

  ret = pbl_vibe_start(VIBE, (int8_t)strength);
  if (ret != 0) {
    pbl_shell_error(sh, "start failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_stop(const struct pbl_shell *sh, size_t argc, char **argv) {
  int ret;

  ret = pbl_vibe_stop(VIBE);
  if (ret != 0) {
    pbl_shell_error(sh, "stop failed (%d)", ret);
  }

  return ret;
}

static int prv_cmd_cal(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint8_t cali;
  int ret;

  ret = pbl_vibe_calibrate(VIBE, &cali);
  if (ret != 0) {
    pbl_shell_error(sh, "calibration failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "calibration: 0x%02x", cali);

  return 0;
}

static const struct pbl_shell_cmd sub_vibe[] = {
  PBL_SHELL_CMD_ARG(start, nullptr, "Vibrate at <strength -100-100>", prv_cmd_start, 2, 0),
  PBL_SHELL_CMD(stop, nullptr, "Stop vibrating", prv_cmd_stop),
  PBL_SHELL_CMD(cal, nullptr, "Calibrate the motor driver", prv_cmd_cal),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(vibe, sub_vibe, "Vibration motor", nullptr);
