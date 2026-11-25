/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/hrm/gh3x2x.h>
#include <pbl/logging/logging.h>

#include <bluetooth/services/services.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

static uint16_t s_tx_handle;
static uint16_t s_rx_handle;
// The connection that wrote last: what the sensor answers goes there.
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static struct ble_gap_event_listener s_listener;

/* {0000190e-0000-1000-8000-00805f9b34fb} */
static const ble_uuid128_t gatt_svr_svc_gh3x2x_ble_uuid = BLE_UUID128_INIT(
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x0e, 0x19, 0x00, 0x00);

/* {00000003-0000-1000-8000-00805f9b34fb} */
static const ble_uuid128_t gatt_svr_chr_gh3x2x_ble_tx_uuid = BLE_UUID128_INIT(
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00);

/* {00000004-0000-1000-8000-00805f9b34fb} */
static const ble_uuid128_t gatt_svr_chr_gh3x2x_ble_rx_uuid = BLE_UUID128_INIT(
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00);

static int gatt_svr_chr_access_gh3x2x_ble_rx(uint16_t conn_handle, uint16_t attr_handle,
                                             struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &gatt_svr_svc_gh3x2x_ble_uuid.u,
    .characteristics =
        (struct ble_gatt_chr_def[]){
          {
            // tx
            .uuid = &gatt_svr_chr_gh3x2x_ble_tx_uuid.u,
            .val_handle = &s_tx_handle,
            .access_cb = gatt_svr_chr_access_gh3x2x_ble_rx,
            .flags = BLE_GATT_CHR_F_NOTIFY,
          },
          {
            .uuid = &gatt_svr_chr_gh3x2x_ble_rx_uuid.u,
            .access_cb = gatt_svr_chr_access_gh3x2x_ble_rx,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            .val_handle = &s_rx_handle,
          },
          {
            0, /* No more characteristics in this service */
          }
        },
  },
  {
    0, /* No more services */
  },
};

// The driver takes a buffer of the C library's heap: a 32-bit length, then the data.
static int prv_data_recv(struct os_mbuf *om) {
  const uint32_t data_len = OS_MBUF_PKTLEN(om);
  uint8_t *buf = malloc(sizeof(data_len) + data_len);

  if (buf == nullptr) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  memcpy(buf, &data_len, sizeof(data_len));
  (void)os_mbuf_copydata(om, 0, (int)data_len, &buf[sizeof(data_len)]);

  if (!gh3x2x_ble_data_recv(buf)) {
    free(buf);
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  return 0;
}

static int gatt_svr_chr_access_gh3x2x_ble_rx(uint16_t conn_handle, uint16_t attr_handle,
                                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
    return BLE_ATT_ERR_UNLIKELY;
  }

  s_conn_handle = conn_handle;

  return prv_data_recv(ctxt->om);
}

void gh3x2x_ble_notify(const uint8_t *p_data, uint32_t data_len) {
  const uint16_t conn_handle = s_conn_handle;
  struct os_mbuf *om;
  int rc;

  if (conn_handle == BLE_HS_CONN_HANDLE_NONE || data_len == 0 || p_data == nullptr) {
    return;
  }

  om = ble_hs_mbuf_from_flat(p_data, data_len);
  if (om == nullptr) {
    return;
  }

  rc = ble_gatts_notify_custom(conn_handle, s_tx_handle, om);
  if (rc != 0) {
    PBL_LOG_DBG("Tuning data not sent: %d", rc);
  }
}

static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  if (event->type == BLE_GAP_EVENT_DISCONNECT &&
      event->disconnect.conn.conn_handle == s_conn_handle) {
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  }

  return 0;
}

int pbl_bt__gh3x2x_tuning_init(void) {
  int rc = ble_gatts_count_cfg(gatt_svr_svcs);
  if (rc != 0) {
    PBL_LOG_ERR("Failed to count GH3X2X tuning service: 0x%04x", (uint16_t)rc);
    return rc;
  }

  rc = ble_gatts_add_svcs(gatt_svr_svcs);
  if (rc != 0) {
    PBL_LOG_ERR("Failed to add GH3X2X tuning service: 0x%04x", (uint16_t)rc);
    return rc;
  }

  /* Listeners outlive the services, which are added each time the stack starts. */
  rc = ble_gap_event_listener_register(&s_listener, prv_gap_event, nullptr);
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    return rc;
  }

  return 0;
}
