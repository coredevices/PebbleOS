/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <host/ble_hs.h>
#include <pbl/shell/shell.h>

static int prv_cmd_host_reset(const struct pbl_shell *sh, size_t argc, char **argv) {
  ble_hs_sched_reset(BLE_HS_EAPP);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, host_reset, NULL, "Reset the host stack", prv_cmd_host_reset, 0, 0);

#endif
