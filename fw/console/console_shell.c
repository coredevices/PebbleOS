/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include "console_internal.h"

#include <errno.h>
#include <stdint.h>

#include <pbl/services/new_timer/new_timer.h>
#include <pbl/shell/shell.h>

static TimerID s_rx_disable_timer = TIMER_INVALID_ID;

static void prv_rx_disable_timer_cb(void *data) {
  serial_console_set_rx_enabled(true);
}

static int prv_cmd_rx_disable(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long seconds;

  if (pbl_shell_strtoul(argv[1], &seconds) != 0 || seconds == 0 || seconds > UINT32_MAX / 1000) {
    pbl_shell_error(sh, "invalid seconds value '%s'", argv[1]);
    return -EINVAL;
  }

  if (s_rx_disable_timer == TIMER_INVALID_ID) {
    s_rx_disable_timer = new_timer_create();
  }

  serial_console_set_rx_enabled(false);

  pbl_shell_print(sh, "console RX disabled for %lu seconds", seconds);

  new_timer_start(s_rx_disable_timer, seconds * 1000, prv_rx_disable_timer_cb, NULL, 0);

  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, rx_disable, NULL, "Disable the console RX <seconds>",
                     prv_cmd_rx_disable, 2, 0);

#endif
