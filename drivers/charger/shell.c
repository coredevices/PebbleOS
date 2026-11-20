/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/drivers/charger.h>
#include <pbl/shell/shell.h>

static const char *const s_status_names[] = {
  [PBL_CHARGER_STATUS_UNKNOWN] = "unknown",
  [PBL_CHARGER_STATUS_FULL] = "full",
  [PBL_CHARGER_STATUS_TRICKLE] = "trickle",
  [PBL_CHARGER_STATUS_CONSTANT_CURRENT] = "constant current",
  [PBL_CHARGER_STATUS_CONSTANT_VOLTAGE] = "constant voltage",
};

static int prv_cmd_charger(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_charger_measurements meas;
  enum pbl_charger_status status;
  bool online;
  int ret;

  ret = pbl_charger_is_online(CHARGER, &online);
  if (ret != 0) {
    pbl_shell_error(sh, "online read failed (%d)", ret);
    return ret;
  }

  ret = pbl_charger_get_status(CHARGER, &status);
  if (ret != 0) {
    pbl_shell_error(sh, "status read failed (%d)", ret);
    return ret;
  }

  ret = pbl_charger_get_measurements(CHARGER, &meas);
  if (ret != 0) {
    pbl_shell_error(sh, "measurement failed (%d)", ret);
    return ret;
  }

  pbl_shell_print(sh, "online: %s", online ? "yes" : "no");
  pbl_shell_print(sh, "status: %s", s_status_names[status]);
  pbl_shell_print(sh, "voltage: %" PRId32 " mV", meas.voltage_mv);
  pbl_shell_print(sh, "current: %" PRId32 " uA", meas.current_ua);
  pbl_shell_print(sh, "temperature: %" PRId32 " mC", meas.temp_mc);

  return 0;
}

PBL_SHELL_CMD_REGISTER(charger, nullptr, "Show the charger state", prv_cmd_charger);
