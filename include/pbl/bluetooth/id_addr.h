/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <nimble/ble.h>

/**
 * @defgroup bluetooth_id_addr Identity address source
 * @ingroup bluetooth
 * @brief Where the watch's identity address comes from.
 *
 * Implemented by the backend chosen with the @c NIMBLE_ID_ADDR_SOURCE Kconfig choice, available
 * when @c NIMBLE_ID_ADDR is enabled. A public address is programmed into the controller by the
 * NimBLE HCI transport, a random static one is set by pbl_bt_start().
 * @{
 */

/**
 * @brief Get the identity address. It is stable across reboots.
 *
 * @param[out] addr The address. Its type is @c BLE_ADDR_PUBLIC for one programmed into the
 * controller, which then reports it as its public address, and @c BLE_ADDR_RANDOM for one set by
 * the host as its random static address.
 * @retval 0 on success.
 * @retval -ENOENT if the source has none: the controller's address is used, or a random static
 *         one where the controller has none.
 * @return Another negative errno otherwise.
 */
int pbl_bt_id_addr_get(ble_addr_t *addr);

/** @} */
