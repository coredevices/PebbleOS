/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/bluetooth/bluetooth_persistent_storage.h>

/**
 * @defgroup services_bluetooth_local_addr Local address
 * @ingroup services_bluetooth
 * @brief Cycling of the local resolvable private address, and the pinned address.
 *
 * A single persistent pinned address is generated once. While cycling is paused (by
 * pairability or by bondings that require address pinning) the pinned address is used on air.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_bt_addr;
/** @endcond */

/**
 * @brief Pause address cycling and use the pinned address (reference counted).
 */
void bt_local_addr_pause_cycling(void);

/**
 * @brief Release a bt_local_addr_pause_cycling() reference; cycling resumes at zero.
 */
void bt_local_addr_resume_cycling(void);

/**
 * @brief Report the local address used when a pairing requested pinning.
 *
 * Called by the Bluetooth driver; only checked against the pinned address and logged.
 *
 * @param addr Local address used during pairing.
 */
void bt_local_addr_pin(const struct pbl_bt_addr *addr);

/**
 * @brief Handle a bonding change.
 *
 * Pauses or resumes cycling depending on whether any bonding requires address pinning.
 *
 * @param bonding Affected bonding.
 * @param op Change made.
 */
void bt_local_addr_handle_bonding_change(pbl_bt_bonding_id_t bonding, BtPersistBondingOp op);

/**
 * @brief Initialize when the stack starts, generating the pinned address if needed.
 */
void bt_local_addr_init(void);

/** @} */
