/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/accessory_transport.h>
#include <pbl/bluetooth/pebble_bt.h>
#include <host/ble_att.h>
#include <host/ble_gatt.h>
#include <host/ble_uuid.h>
#include <system/passert.h>

#include "nimble_type_conversions.h"

static uint16_t s_tx_notify_handle;
static uint16_t s_rx_write_handle;

static int prv_access_tx(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
  return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
}

static int prv_access_rx_write(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
  return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
}

#include "accessory_transport_gatt.h"

void accessory_transport_service_init(void) {
  int rc = ble_gatts_count_cfg(s_an_transport_svc);
  PBL_ASSERTN(rc == 0);
  rc = ble_gatts_add_svcs(s_an_transport_svc);
  PBL_ASSERTN(rc == 0);
}

void accessory_transport_service_set_handler(AccessoryTransportNotificationHandler handler) {
}

bool accessory_transport_service_send_response(const char *feature_id, size_t feature_id_len,
                                               const uint8_t *payload, size_t payload_len,
                                               AccessoryTransportSendComplete complete, void *ctx) {
  // Recovery has no notification consumer or key store; never accept work it cannot finish.
  return false;
}
