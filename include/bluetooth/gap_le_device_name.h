/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gap_le_connection.h"

//! Bluetooth LE GAP Device name APIs
void bt_driver_gap_le_device_name_request_all(void);
void bt_driver_gap_le_device_name_request(const BTDeviceInternal *address);

//! The caller is expected to have implemented:
//! ctx will be kernel_free()'d
void bt_driver_store_device_name_kernelbg_cb(void *ctx);
