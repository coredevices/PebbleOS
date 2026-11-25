/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup services_bluetooth_pairability Pairability
 * @ingroup services_bluetooth
 * @brief Reference-counted requests to be discoverable and pairable over BLE.
 *
 * The watch is discoverable and pairable while the count is non-zero. The service itself holds a
 * reference while the watch has no bonding, so that an unpaired watch can be found. References may
 * be taken and released from any thread; changes are applied from SysWorkq.
 * @{
 */

/** @brief Take a pairability reference. */
void bt_pairability_use(void);

/** @brief Release a pairability reference. */
void bt_pairability_release(void);

/**
 * @brief Be pairable for a period.
 *
 * Takes one reference, released when the period ends. Calling again before then restarts the
 * period without taking another reference.
 *
 * @param duration_secs Period in seconds; 0 ends a period that is running.
 */
void bt_pairability_use_for_period(uint16_t duration_secs);

/** @brief Re-evaluate the reference held while there is no bonding, after the bondings changed. */
void bt_pairability_update_due_to_bonding_change(void);

/** @brief Initialize pairability when the stack starts. */
void bt_pairability_init(void);

/** @} */
