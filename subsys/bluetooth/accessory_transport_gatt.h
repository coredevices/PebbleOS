/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Keep the notification service definition identical in normal and recovery firmware.
static const struct ble_gatt_svc_def s_an_transport_svc[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = BLE_UUID128_DECLARE(
        BLE_UUID_SWIZZLE(PBL_BT_PEBBLE_UUID_EXPAND(PBL_BT_PEBBLE_AN_TRANSPORT_SERVICE_UUID_32BIT))),
    .characteristics =
        (struct ble_gatt_chr_def[]){
          {
            .uuid = BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PBL_BT_PEBBLE_UUID_EXPAND(
                PBL_BT_PEBBLE_AN_TRANSPORT_TX_CHARACTERISTIC_UUID_32BIT))),
            .access_cb = prv_access_tx,
            // READ_ENC gates both the read and the CCCD write to an encrypted link.
            .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            .val_handle = &s_tx_notify_handle,
          },
          {
            .uuid = BLE_UUID128_DECLARE(BLE_UUID_SWIZZLE(PBL_BT_PEBBLE_UUID_EXPAND(
                PBL_BT_PEBBLE_AN_TRANSPORT_RX_CHARACTERISTIC_UUID_32BIT))),
            .access_cb = prv_access_rx_write,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            .val_handle = &s_rx_write_handle,
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
