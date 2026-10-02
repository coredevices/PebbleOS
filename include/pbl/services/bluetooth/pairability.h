/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_bluetooth_pairability Pairability
 * @ingroup services_bluetooth
 * @brief Reference-counted requests to be discoverable and pairable over BLE.
 *
 * The watch is discoverable and pairable while the count is non-zero; address cycling is
 * paused meanwhile. Changes are applied from KernelBG.
 * @{
 */

/**
 * @brief Take a pairability reference.
 *
 * Same as bt_pairability_use_ble().
 */
void bt_pairability_use(void);

/**
 * @brief Take a BLE pairability reference.
 */
void bt_pairability_use_ble(void);

/**
 * @brief Be pairable for a period.
 *
 * Takes one reference, released automatically when the period ends. Calling again before then
 * restarts the period without taking another reference.
 *
 * @param duration_secs Period in seconds.
 */
void bt_pairability_use_ble_for_period(uint16_t duration_secs);

/**
 * @brief Release a pairability reference.
 *
 * Same as bt_pairability_release_ble().
 */
void bt_pairability_release(void);

/**
 * @brief Release a BLE pairability reference.
 */
void bt_pairability_release_ble(void);

/**
 * @brief Re-evaluate pairability after the bondings changed.
 *
 * Holds a reference while there is no gateway or ANCS bonding, so an unpaired watch can be
 * found.
 */
void bt_pairability_update_due_to_bonding_change(void);

/**
 * @brief Initialize pairability when the stack starts.
 */
void bt_pairability_init(void);

/** @} */
