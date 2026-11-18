/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

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

/** @brief How the identity address is applied. */
enum pbl_bt_id_addr_type {
  /** Programmed into the controller, which then reports it as its public address. */
  PBL_BT_ID_ADDR_PUBLIC,
  /** Set by the host as its random static address. */
  PBL_BT_ID_ADDR_RANDOM_STATIC,
};

/**
 * @brief Get the identity address. It is stable across reboots.
 *
 * @param[out] addr The address.
 * @param[out] type How the address is applied.
 * @retval 0 on success.
 * @retval -ENOENT if the source has none: the controller's address is used.
 * @return Another negative errno otherwise.
 */
int pbl_bt_id_addr_get(struct pbl_bt_addr *addr, enum pbl_bt_id_addr_type *type);

/** @} */
