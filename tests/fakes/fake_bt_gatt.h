/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/gatt.h>

//! @file fake_bt_gatt.h
//! Fake of the Service Changed part of pbl/bluetooth/gatt.h: it records the indications the
//! firmware sends.

void fake_gatt_init(void);

//! @return Number of Service Changed indications sent since fake_gatt_init().
int fake_gatt_get_service_changed_indication_count(void);

//! @return The device the last Service Changed indication was sent to.
const struct pbl_bt_device_internal *fake_gatt_get_service_changed_last_device(void);

//! @return The handle range of the last Service Changed indication.
struct pbl_bt_att_handle_range fake_gatt_get_service_changed_last_range(void);
