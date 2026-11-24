/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <errno.h>
#include <stdint.h>

#include <pbl/bluetooth/responsiveness.h>
#include <pbl/shell/shell.h>

#include <comm/ble/gap_le_connection.h>
#include <comm/bt_lock.h>

extern void hc_endpoint_logging_set_level(uint8_t level);
extern bool hc_endpoint_logging_get_level(uint8_t *level);

static int prv_parse_u16(const struct pbl_shell *sh, const char *str, uint16_t *out) {
  unsigned long val;

  if (pbl_shell_strtoul(str, &val) != 0 || val > UINT16_MAX) {
    pbl_shell_error(sh, "invalid value '%s'", str);
    return -EINVAL;
  }

  *out = val;
  return 0;
}

static GAPLEConnection *prv_get_le_connection_and_print_info(const struct pbl_shell *sh) {
  GAPLEConnection *conn = gap_le_connection_any();
  if (!conn) {
    pbl_shell_print(sh, "no device connected");
  } else {
    pbl_shell_print(sh, "connected to " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(conn->device.address));
  }

  return conn;
}

static int prv_cmd_conn_params(const struct pbl_shell *sh, size_t argc, char **argv) {
  struct pbl_bt_conn_params_update_req req;
  uint16_t val[4];

  for (size_t i = 0; i < 4; i++) {
    if (prv_parse_u16(sh, argv[i + 1], &val[i]) != 0) {
      return -EINVAL;
    }
  }

  req.interval_min_1_25ms = val[0];
  req.interval_max_1_25ms = val[1];
  req.slave_latency_events = val[2];
  req.supervision_timeout_10ms = val[3];

  GAPLEConnection *conn = prv_get_le_connection_and_print_info(sh);
  struct pbl_bt_device_internal addr = {};
  if (conn) {
    addr.address = conn->device.address;
  }

  pbl_bt_le_connection_parameter_update(&addr, &req);
  return 0;
}

static int prv_cmd_log_level(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint8_t level;

  if (argc < 2) {
    if (!hc_endpoint_logging_get_level(&level)) {
      pbl_shell_error(sh, "unable to get the BLE log level");
      return -EIO;
    }
    pbl_shell_print(sh, "BLE log level: %d", level);
    return 0;
  }

  unsigned long val;
  if (pbl_shell_strtoul(argv[1], &val) != 0 || val > UINT8_MAX) {
    pbl_shell_error(sh, "invalid level '%s'", argv[1]);
    return -EINVAL;
  }

  hc_endpoint_logging_set_level(val);
  pbl_shell_print(sh, "BLE log level set to: %lu", val);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, conn_params, nullptr,
                     "Request connection parameters <min_1.25ms> <max_1.25ms> <latency> "
                     "<timeout_10ms>",
                     prv_cmd_conn_params, 5, 0);
PBL_SHELL_SUBCMD_ADD(sub_bt, log_level, nullptr, "Get or set the BLE log level [level]",
                     prv_cmd_log_level, 1, 1);

#endif
