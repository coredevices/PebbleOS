/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "os/endian.h"
#include "console/console.h"
#include "drivers/hrm/gh3x2x/gh3x2x.h"


uint16_t g_gh3x2x_ble_attr_tx_handle;
uint16_t g_gh3x2x_ble_attr_rx_handle;
uint16_t g_gh3x2x_ble_conn_handle = 0xffff;

/* {0000190e-0000-1000-8000-00805f9b34fb} */
static const ble_uuid128_t gatt_svr_svc_gh3x2x_ble_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x0e, 0x19, 0x00, 0x00);

/* {00000003-0000-1000-8000-00805f9b34fb} */
static const ble_uuid128_t gatt_svr_chr_gh3x2x_ble_tx_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00);

/* {00000004-0000-1000-8000-00805f9b34fb} */
static const ble_uuid128_t gatt_svr_chr_gh3x2x_ble_rx_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00);

static int gatt_svr_chr_access_gh3x2x_ble_rx(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
void gh3x2x_ble_notify(const uint8_t* p_data, uint32_t data_len);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &gatt_svr_svc_gh3x2x_ble_uuid.u,
    .characteristics = (struct ble_gatt_chr_def[]) { 
      {
        // tx
        .uuid = &gatt_svr_chr_gh3x2x_ble_tx_uuid.u,
        .val_handle = &g_gh3x2x_ble_attr_tx_handle,
        .access_cb = gatt_svr_chr_access_gh3x2x_ble_rx,
        .flags = BLE_GATT_CHR_F_NOTIFY,
      },{
        .uuid = &gatt_svr_chr_gh3x2x_ble_rx_uuid.u,
        .access_cb = gatt_svr_chr_access_gh3x2x_ble_rx,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        .val_handle = &g_gh3x2x_ble_attr_rx_handle,
      },{
        0, /* No more characteristics in this service */
      } 
    },
  },
  {
    0, /* No more services */
  },
};

static void ble_gh3x2x_ble_data_recv_handle(struct os_mbuf *om){
  uint32_t data_len = 0;
  struct os_mbuf *first = om;
  while(om) {
    data_len += om->om_len;
    om = SLIST_NEXT(om, om_next);
  }
  om = first;

  uint8_t* p_data;
  uint8_t* p_start = (uint8_t*)malloc(data_len + sizeof(uint32_t));
  if (p_start == NULL) {
    return;
  }
  p_data = p_start;
  memcpy(p_data, &data_len, sizeof(uint32_t));
  p_data += sizeof(uint32_t);
  while (om) {
    memcpy(p_data, om->om_data, om->om_len);
    p_data += om->om_len;
    om = SLIST_NEXT(om, om_next);
  }

  if (!gh3x2x_ble_data_recv(p_start)) { 
    free(p_start);
  }
}

static int gatt_svr_chr_access_gh3x2x_ble_rx(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
  g_gh3x2x_ble_conn_handle = conn_handle;
  struct os_mbuf *om = ctxt->om;
  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
      ble_gh3x2x_ble_data_recv_handle(om);
      return 0;
    }
    default: {
      return BLE_ATT_ERR_UNLIKELY;
    }
  }
}

void gh3x2x_ble_notify(const uint8_t* p_data, uint32_t data_len) {
  if (0xffff == g_gh3x2x_ble_conn_handle || data_len == 0 || p_data == NULL) {
    return;
  }

  struct os_mbuf *om = ble_hs_mbuf_from_flat(p_data, data_len);
  if (!om) {
      return;
  }
  ble_gatts_notify_custom(g_gh3x2x_ble_conn_handle, g_gh3x2x_ble_attr_tx_handle, om);
}

int gh3x2x_tuning_service_init(void) {
  int rc = ble_gatts_count_cfg(gatt_svr_svcs);
  if (rc != 0) {
    return rc;
  }
  rc = ble_gatts_add_svcs(gatt_svr_svcs);
  if (rc != 0) {
    return rc;
  }
  return rc;
}

