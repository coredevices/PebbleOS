/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

//! Callback that is called for each advertisement that is found while scanning
//! using ble_scan_start().
//! @param device The device from which the advertisement originated.
//! @param rssi The RSSI (Received Signal Strength Indication) of the
//! advertisement.
//! @param advertisement_data The payload of the advertisement. When there was
//! a scan response, this payload will contain the data of the scan response
//! as well. The information in the payload can be accessed using the
//! ble_ad_... functions, @see for example ble_ad_copy_local_name() and
//! ble_ad_includes_service().
//! @note The advertisement_data is cleaned up by the system automatically
//! immediately after returning from this callback. Do not keep around
//! any long-lived references around to the advertisement_data.
//! @note Do not use ble_ad_destroy() on the advertisement_data.
typedef void (*BLEScanHandler)(struct pbl_bt_device device, int8_t rssi,
                               const struct pbl_bt_ad_data *advertisement_data);

//! Start scanning for advertisements. Pebble will scan actively, meaning it
//! will perform scan requests whenever the advertisement is scannable.
//! @param handler The callback to handle the found advertisements. It must not
//! be NULL.
//! @return PBL_BT_ERRNO_OK if scanning started successfully, PBL_BT_ERRNO_INVALID_PARAMETER
//! if the handler was invalid or PBL_BT_ERRNO_INVALID_STATE if scanning had already
//! been started.
enum pbl_bt_errno ble_scan_start(BLEScanHandler handler);

//! Stop scanning for advertisements.
//! @return PBL_BT_ERRNO_OK if scanning stopped successfully, or TODO...
enum pbl_bt_errno ble_scan_stop(void);

//! @return True if the system is scanning for advertisements or false if not.
bool ble_scan_is_scanning(void);
