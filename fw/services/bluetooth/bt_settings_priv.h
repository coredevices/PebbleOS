/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/bluetooth/types.h>

//! Get the peer name kept with the shared PRF storage bond.
bool bt_settings__sprf_get_peer_name(const struct pbl_bt_device_internal *addr,
                                     char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE]);

//! Store the peer name with the shared PRF storage bond, if it is the bond of @p addr.
void bt_settings__sprf_set_peer_name(const struct pbl_bt_device_internal *addr, const char *name);
