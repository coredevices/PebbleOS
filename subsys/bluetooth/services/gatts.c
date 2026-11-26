/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/bluetooth/gatts.h>

#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <os/os_mbuf.h>

int pbl_bt_gatts_add(const struct ble_gatt_svc_def *svcs) {
  int rc;

  rc = ble_gatts_count_cfg(svcs);
  if (rc != 0) {
    return -EINVAL;
  }

  rc = ble_gatts_add_svcs(svcs);
  if (rc != 0) {
    return (rc == BLE_HS_ENOMEM) ? -ENOMEM : -EIO;
  }

  return 0;
}

int pbl_bt_gatts_notify(uint16_t conn_handle, uint16_t attr_handle, const void *data,
                        uint16_t len) {
  struct os_mbuf *om;

  om = ble_hs_mbuf_from_flat(data, len);
  if (om == nullptr) {
    return -ENOMEM;
  }

  /* The buffer is consumed, whatever the outcome. */
  switch (ble_gatts_notify_custom(conn_handle, attr_handle, om)) {
    case 0:
      return 0;
    case BLE_HS_ENOMEM:
      return -ENOMEM;
    case BLE_HS_ENOTCONN:
      return -ENOTCONN;
    default:
      return -EIO;
  }
}
