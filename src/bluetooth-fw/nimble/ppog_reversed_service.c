/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ppog_reversed_service.h"

#include <bluetooth/bt_driver_ppog_reversed.h>
#include <bluetooth/pebble_bt.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <kernel/pbl_malloc.h>
#include <os/os_mbuf.h>
#include <system/logging.h>
#include <system/passert.h>
#include <util/uuid.h>

#include "nimble_type_conversions.h"

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
  // Heap-allocate (not stack — this runs on the 5000-byte NimBLE host task) and
  // hand ownership to the kernel callback, which will free it after the
  // KernelBG handler runs.
  const uint16_t pkt_len = OS_MBUF_PKTLEN(ctxt->om);
  if (pkt_len == 0) {
    return 0;
  }
  uint8_t *buf = kernel_malloc(pkt_len);
  if (!buf) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  uint16_t out_len = 0;
  int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, pkt_len, &out_len);
  if (rc != 0) {
    PBL_LOG_ERR("Reversed PPoG write flatten failed: 0x%04x", (uint16_t) rc);
    kernel_free(buf);
    return BLE_ATT_ERR_UNLIKELY;
  }
  bt_driver_cb_ppog_reversed_data_written(conn_handle, buf, out_len);
  return 0;
}

static const struct ble_gatt_svc_def s_ppog_reversed_svc[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(
            PEBBLE_BT_UUID_EXPAND(PEBBLE_BT_PPOGATT_WATCH_SERVER_SERVICE_UUID_32BIT))),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PEBBLE_BT_UUID_EXPAND(
                        PEBBLE_BT_PPOGATT_WATCH_SERVER_DATA_CHARACTERISTIC_UUID_32BIT))),
                    .access_cb = prv_access_data_notify,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &s_data_notify_handle,
                },
                {
                    .uuid = BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PEBBLE_BT_UUID_EXPAND(
                        PEBBLE_BT_PPOGATT_WATCH_SERVER_DATA_WR_CHARACTERISTIC_UUID_32BIT))),
                    .access_cb = prv_access_data_write,
                    .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
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

void ppog_reversed_service_init(void) {
  int rc = ble_gatts_count_cfg(s_ppog_reversed_svc);
  PBL_ASSERTN(rc == 0);
  rc = ble_gatts_add_svcs(s_ppog_reversed_svc);
  PBL_ASSERTN(rc == 0);
}

void ppog_reversed_service_handle_subscribe_event(struct ble_gap_event *event) {
  if (event->subscribe.attr_handle != s_data_notify_handle) {
    return;
  }
  if (event->subscribe.cur_notify == event->subscribe.prev_notify) {
    return;
  }
  if (event->subscribe.cur_notify) {
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(event->subscribe.conn_handle, &desc) != 0) {
      PBL_LOG_ERR("Reversed PPoG subscribe: no conn descriptor for handle %u",
                  event->subscribe.conn_handle);
      return;
    }
    BTDeviceInternal device;
    nimble_addr_to_pebble_device(&desc.peer_id_addr, &device);
    bt_driver_cb_ppog_reversed_subscribed(&device, event->subscribe.conn_handle);
  } else {
    bt_driver_cb_ppog_reversed_unsubscribed(event->subscribe.conn_handle);
  }
}

void ppog_reversed_service_handle_notify_tx_event(struct ble_gap_event *event) {
  if (event->notify_tx.attr_handle != s_data_notify_handle) {
    return;
  }
  // Successful TX frees buffer space; let the kernel drain any backlog.
  if (event->notify_tx.status == 0 || event->notify_tx.status == BLE_HS_EDONE) {
    bt_driver_cb_ppog_reversed_notify_tx_complete();
  }
}

BTErrno bt_driver_ppog_reversed_notify(uint16_t conn_handle,
                                       const uint8_t *buf, uint16_t len) {
  struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
  if (!om) {
    return BTErrnoNotEnoughResources;
  }
  int rc = ble_gatts_notify_custom(conn_handle, s_data_notify_handle, om);
  // ble_gatts_notify_custom always consumes the mbuf on enqueue. On error, it
  // also frees it (NimBLE handles ownership internally).
  switch (rc) {
    case 0:
      return BTErrnoOK;
    case BLE_HS_ENOMEM:
      return BTErrnoNotEnoughResources;
    case BLE_HS_ENOTCONN:
      return BTErrnoInvalidState;
    default:
      PBL_LOG_ERR("ble_gatts_notify_custom failed: 0x%04x", (uint16_t) rc);
      return (BTErrno)(BTErrnoInternalErrorBegin + rc);
  }
}
