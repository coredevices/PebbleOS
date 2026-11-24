/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/vibe.h>
#include <pbl/kernel/delay.h>
#include <pbl/shell/shell.h>

#define TAP_MAX_MS 1000

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

static int prv_parse_ms(const struct pbl_shell *sh, const char *arg, unsigned long *ms) {
  if (pbl_shell_strtoul(arg, ms) != 0 || *ms > TAP_MAX_MS) {
    pbl_shell_error(sh, "invalid duration '%s'", arg);
    return -EINVAL;
  }

  return 0;
}

// One pulse, then optionally driven in reverse, which stops an LRA sooner
static int prv_cmd_tap(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long drive_ms;
  unsigned long brake_ms = 0;
  long strength;
  int ret;

  if (pbl_shell_strtol(argv[1], &strength) != 0 || strength <= VIBE_STRENGTH_OFF ||
      strength > VIBE_STRENGTH_MAX) {
    pbl_shell_error(sh, "invalid strength '%s'", argv[1]);
    return -EINVAL;
  }
  if (prv_parse_ms(sh, argv[2], &drive_ms) != 0 ||
      (argc > 3 && prv_parse_ms(sh, argv[3], &brake_ms) != 0)) {
    return -EINVAL;
  }

  ret = pbl_vibe_start(VIBE, (int8_t)strength);
  if (ret == 0) {
    pbl_sleep_ms(drive_ms);
    if (brake_ms > 0U) {
      ret = pbl_vibe_start(VIBE, (int8_t)-strength);
      pbl_sleep_ms(brake_ms);
    }
  }
  (void)pbl_vibe_stop(VIBE);

  if (ret != 0) {
    pbl_shell_error(sh, "tap failed (%d)", ret);
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
  PBL_SHELL_CMD_ARG(tap, nullptr,
                    "Pulse at <strength 1-100> for <drive-ms>, reversed for [brake-ms]",
                    prv_cmd_tap, 3, 1),
  PBL_SHELL_CMD(cal, nullptr, "Calibrate the motor driver", prv_cmd_cal),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(vibe, sub_vibe, "Vibration motor", nullptr);
