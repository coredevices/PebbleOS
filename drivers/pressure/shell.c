/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/pressure.h>
#include <pbl/shell/shell.h>

static int prv_cmd_pressure(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t pa;
  int ret;

  ret = pbl_pressure_read(PRESSURE, &pa);
  if (ret != 0) {
    pbl_shell_error(sh, "read failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "%" PRIu32 " Pa (%" PRIu32 ".%02" PRIu32 " hPa)", pa, pa / 100U, pa % 100U);

  return 0;
}

PBL_SHELL_CMD_REGISTER(pressure, nullptr, "Measure the barometric pressure", prv_cmd_pressure);
