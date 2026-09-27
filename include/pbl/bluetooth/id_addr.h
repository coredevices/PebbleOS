/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

enum pbl_bt_id_addr_type {
  //! Programmed into the controller, which then reports it as its public address.
  PBL_BT_ID_ADDR_PUBLIC,
  //! Set by the host as its random static address.
  PBL_BT_ID_ADDR_RANDOM_STATIC,
};

//! Gets the identity address the Bluetooth stack should use. It is stable across reboots.
//! @param[out] addr The address.
//! @param[out] type How the address is applied.
//! @return 0 on success, a negative errno otherwise.
int pbl_bt_id_addr_get(struct pbl_bt_addr *addr, enum pbl_bt_id_addr_type *type);
