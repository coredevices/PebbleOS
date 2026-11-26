/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/clients/service_changed.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>

#include <host/ble_gatt.h>
#include <services/gatt/ble_svc_gatt.h>

static const ble_uuid16_t s_svc_uuid = BLE_UUID16_INIT(BLE_GATT_SVC_UUID16);
static const ble_uuid16_t s_chr_uuid = BLE_UUID16_INIT(BLE_SVC_GATT_CHR_SERVICE_CHANGED_UUID16);
static const ble_uuid_t *const s_chr_uuids[] = {&s_chr_uuid.u};

static struct pbl_bt_service_changed_client *prv_client(struct pbl_bt_gattc_service *svc) {
  return PBL_CONTAINER_OF(svc, struct pbl_bt_service_changed_client, svc);
}

static void prv_discovered(struct pbl_bt_gattc_service *svc, int status) {
  if (status == 0) {
    (void)pbl_bt_gattc_service_subscribe(svc, 0, true);
  }
}

static void prv_notified(struct pbl_bt_gattc_service *svc, uint8_t chr, const uint8_t *data,
                         uint16_t len) {
  struct pbl_bt_service_changed_client *client = prv_client(svc);

  if (len != 2 * sizeof(uint16_t)) {
    return;
  }

  client->cb(client, pbl_get_le16(&data[0]), pbl_get_le16(&data[2]));
}

static const struct pbl_bt_gattc_service_cb s_cb = {
  .discovered = prv_discovered,
  .notified = prv_notified,
};

int pbl_bt_service_changed_client_start(struct pbl_bt_service_changed_client *client,
                                        uint16_t conn_handle, pbl_bt_service_changed_cb_t cb) {
  client->cb = cb;
  client->svc.svc_uuid = &s_svc_uuid.u;
  client->svc.chr_uuids = s_chr_uuids;
  client->svc.num_chrs = 1;
  client->svc.notifying_chrs = 1U << 0;
  client->svc.chrs = client->chrs;

  return pbl_bt_gattc_service_start(&client->svc, conn_handle, &s_cb);
}

void pbl_bt_service_changed_client_stop(struct pbl_bt_service_changed_client *client) {
  pbl_bt_gattc_service_stop(&client->svc);
}

bool pbl_bt_service_changed_client_notify_rx(struct pbl_bt_service_changed_client *client,
                                             uint16_t conn_handle, uint16_t attr_handle,
                                             struct os_mbuf *om) {
  return pbl_bt_gattc_service_notify_rx(&client->svc, conn_handle, attr_handle, om);
}
