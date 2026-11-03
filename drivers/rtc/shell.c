/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/drivers/rtc.h>
#include <pbl/shell/shell.h>

static int prv_cmd_get(const struct pbl_shell *sh, size_t argc, char **argv) {
  time_t t;
  int ret;

  ret = pbl_rtc_get_time(RTC, &t);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%" PRId64, (int64_t)t);

  return 0;
}

static int prv_cmd_set(const struct pbl_shell *sh, size_t argc, char **argv) {
  long t;
  int ret;

  if (pbl_shell_strtol(argv[1], &t) != 0 || t < 0) {
    pbl_shell_error(sh, "invalid time '%s'", argv[1]);
    return -EINVAL;
  }

  ret = pbl_rtc_set_time(RTC, (time_t)t);
  if (ret != 0) {
    pbl_shell_error(sh, "write failed (%d)", ret);
  }

  return ret;
}

static const struct pbl_shell_cmd sub_rtc[] = {
  PBL_SHELL_CMD(get, nullptr, "Read the time, UTC seconds since the epoch", prv_cmd_get),
  PBL_SHELL_CMD_ARG(set, nullptr, "Set the time <UTC seconds since the epoch>", prv_cmd_set, 2, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(rtc, sub_rtc, "RTC", nullptr);
