/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/console/console.h>
#include <pbl/kernel/work.h>
#include <pbl/shell/shell.h>

static void prv_rx_disable_timer_cb(struct pbl_work *work) {
  pbl_console_set_rx_enabled(true);
}

static PBL_DELAYED_WORK_DEFINE(s_rx_disable_timer, prv_rx_disable_timer_cb);

static int prv_cmd_rx_disable(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long seconds;

  if (pbl_shell_strtoul(argv[1], &seconds) != 0 || seconds == 0 || seconds > UINT32_MAX / 1000) {
    pbl_shell_error(sh, "invalid seconds value '%s'", argv[1]);
    return -EINVAL;
  }

  pbl_console_set_rx_enabled(false);

  pbl_shell_print(sh, "console RX disabled for %lu seconds", seconds);

  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_rx_disable_timer, PBL_MSEC(seconds * 1000));

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, rx_disable, nullptr, "Disable the console RX <seconds>",
                     prv_cmd_rx_disable, 2, 0);
