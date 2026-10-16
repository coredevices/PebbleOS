/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/temperature.h>
#include <pbl/shell/shell.h>

static int prv_cmd_temp(const struct pbl_shell *sh, size_t argc, char **argv) {
  int32_t millicelsius;
  int ret;

  ret = pbl_temp_read(DIE_TEMP, &millicelsius);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%" PRId32, millicelsius);

  return 0;
}

PBL_SHELL_CMD_REGISTER(temp, nullptr, "Read the die temperature", prv_cmd_temp);
