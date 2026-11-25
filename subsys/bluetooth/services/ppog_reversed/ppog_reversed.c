/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/bluetooth/pebble_bt.h>
#include <pbl/bluetooth/ppog_reversed.h>
#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>

#include <bluetooth/services/services.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

static uint16_t s_data_notify_handle;
static uint16_t s_data_write_handle;

static int prv_access_data_notify(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
  // Notify-only — refuse explicit reads.
  return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

static int prv_access_data_write(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
    return BLE_ATT_ERR_UNLIKELY;
  }
  // Flatten into a kernel-heap buffer; ownership passes to the kernel callback.
  const uint16_t pkt_len = OS_MBUF_PKTLEN(ctxt->om);
  if (pkt_len == 0) {
    return 0;
  }
  uint8_t *buf = pbl_malloc(pkt_len);
  if (!buf) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  uint16_t out_len = 0;
  int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, pkt_len, &out_len);
  if (rc != 0) {
    PBL_LOG_ERR("Reversed PPoG write flatten failed: 0x%04x", (uint16_t)rc);
    pbl_free(buf);
    return BLE_ATT_ERR_UNLIKELY;
  }
  pbl_bt_cb_ppog_reversed_data_written(conn_handle, buf, out_len);
  return 0;
}

static const ble_uuid128_t s_svc_uuid =
    PBL_BT_PEBBLE_UUID128_INIT(PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_SERVICE_UUID_32BIT);
static const ble_uuid128_t s_data_uuid =
    PBL_BT_PEBBLE_UUID128_INIT(PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_DATA_CHARACTERISTIC_UUID_32BIT);
static const ble_uuid128_t s_data_wr_uuid = PBL_BT_PEBBLE_UUID128_INIT(
    PBL_BT_PEBBLE_PPOGATT_WATCH_SERVER_DATA_WR_CHARACTERISTIC_UUID_32BIT);

static const struct ble_gatt_svc_def s_ppog_reversed_svc[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &s_svc_uuid.u,
    .characteristics =
        (struct ble_gatt_chr_def[]){
          {
            .uuid = &s_data_uuid.u,
            .access_cb = prv_access_data_notify,
            // READ_ENC (without READ) gates the CCCD: subscribing
            // requires an encrypted link, explicit reads stay blocked.
            .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_ENC,
            .val_handle = &s_data_notify_handle,
          },
          {
            .uuid = &s_data_wr_uuid.u,
            .access_cb = prv_access_data_write,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
            .val_handle = &s_data_write_handle,
          },
          {
            0,
          },
        },
  },
  {
    0,
  },
};

static void prv_handle_subscribe_event(struct ble_gap_event *event) {
  if (event->subscribe.attr_handle != s_data_notify_handle) {
    return;
  }
  if (event->subscribe.cur_notify == event->subscribe.prev_notify) {
    return;
  }
  if (event->subscribe.cur_notify) {
    pbl_bt_cb_ppog_reversed_subscribed(event->subscribe.conn_handle);
  } else {
    pbl_bt_cb_ppog_reversed_unsubscribed(event->subscribe.conn_handle);
  }
}

static int prv_handle_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_SUBSCRIBE:
      prv_handle_subscribe_event(event);
      break;
    default:
      break;
  }
  return 0;
}

static struct ble_gap_event_listener s_gap_event_listener;

int pbl_bt__ppog_reversed_init(void) {
  int rc = ble_gatts_count_cfg(s_ppog_reversed_svc);
  if (rc != 0) {
    PBL_LOG_ERR("Failed to count reversed PPoG service: 0x%04x", (uint16_t)rc);
    return rc;
  }

  rc = ble_gatts_add_svcs(s_ppog_reversed_svc);
  if (rc != 0) {
    PBL_LOG_ERR("Failed to add reversed PPoG service: 0x%04x", (uint16_t)rc);
    return rc;
  }

  // Called on every pbl_bt_start, but the listener list is only cleared on
  // nimble_port_init, so tolerate EALREADY.
  rc = ble_gap_event_listener_register(&s_gap_event_listener, prv_handle_gap_event, nullptr);
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    PBL_LOG_ERR("Failed to register reversed PPoG GAP listener: 0x%04x", (uint16_t)rc);
    return rc;
  }

  return 0;
}

int pbl_bt_ppog_reversed_notify(uint16_t conn_handle, const uint8_t *buf, uint16_t len) {
  struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
  if (!om) {
    return -ENOMEM;
  }
  int rc = ble_gatts_notify_custom(conn_handle, s_data_notify_handle, om);
  // ble_gatts_notify_custom always consumes the mbuf, even on error.
  switch (rc) {
    case 0:
      return 0;
    case BLE_HS_ENOMEM:
      return -ENOMEM;
    case BLE_HS_ENOTCONN:
      return -ENOTCONN;
    default:
      PBL_LOG_ERR("ble_gatts_notify_custom failed: 0x%04x", (uint16_t)rc);
      return -EIO;
  }
}
