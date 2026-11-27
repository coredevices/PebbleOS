/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "op_queue.h"

#include <pbl/bluetooth/clients/gap.h>
#include <pbl/bluetooth/gap.h>
#include <pbl/kernel/malloc.h>
#include <pbl/util/math.h>

#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <os/os_mbuf.h>

struct read {
  uint16_t conn_handle;
  pbl_bt_gapc_name_cb_t cb;
};

static const ble_uuid16_t s_name_uuid = BLE_UUID16_INIT(0x2A00);

static int prv_read_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                       struct ble_gatt_attr *attr, void *arg) {
  const struct read *read = arg;
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  size_t len;

  if (error->status != 0) {
    pbl_bt__gattc_op_queue_complete();
    return 0;
  }

  len = PBL_MIN(OS_MBUF_PKTLEN(attr->om), sizeof(name) - 1);
  (void)os_mbuf_copydata(attr->om, 0, len, name);
  name[len] = '\0';
  read->cb(conn_handle, name);

  return 0;
}

static int prv_start(void *ctx) {
  const struct read *read = ctx;

  return ble_gattc_read_by_uuid(read->conn_handle, 1, UINT16_MAX, &s_name_uuid.u, prv_read_cb, ctx);
}

void pbl_bt_gapc_read_name(uint16_t conn_handle, pbl_bt_gapc_name_cb_t cb) {
  struct read *read = pbl_malloc(sizeof(*read));

  if (read == nullptr) {
    return;
  }

  *read = (struct read){
    .conn_handle = conn_handle,
    .cb = cb,
  };
  if (pbl_bt__gattc_op_queue_push(prv_start, read) != 0) {
    pbl_free(read);
  }
}
