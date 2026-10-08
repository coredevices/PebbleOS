/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/temperature.h>
#include <pbl/shell/shell.h>

static int prv_cmd_temp(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "%" PRId32, temperature_read());
  return 0;
}

PBL_SHELL_CMD_REGISTER(temp, NULL, "Read the temperature", prv_cmd_temp);
