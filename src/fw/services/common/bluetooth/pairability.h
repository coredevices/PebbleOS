/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Reference counted request to allow us to be discovered and paired with over BT Classic & LE.
void bt_pairability_use(void);

//! Reference counted request to allow us to be discovered and paired with over BT Classic.
void bt_pairability_use_bt(void);

//! Reference counted request to allow us to be discovered and paired with over BLE.
void bt_pairability_use_ble(void);

//! Reference counted request to allow us to be discovered and paired with over BLE for a specific
//! period, after which bt_pairability_release_ble will be called automatically.
void bt_pairability_use_ble_for_period(uint16_t duration_secs);

//! Reference counted request to disallow us to be discovered and paired with over BT Classic & LE.
void bt_pairability_release(void);

//! Reference counted request to disallow us to be discovered and paired with over BT Classic.
void bt_pairability_release_bt(void);

//! Reference counted request to disallow us to be discovered and paired with over BLE.
void bt_pairability_release_ble(void);

//! Evaluates whether there are any bondings to gateways. If there are none, make the system
//! discoverable and pairable.
void bt_pairability_update_due_to_bonding_change(void);

void bt_pairability_init(void);
