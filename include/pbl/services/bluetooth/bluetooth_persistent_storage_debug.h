/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/sm_types.h>
#include <pbl/btutil/sm_util.h>

struct pbl_shell;

void bluetooth_persistent_storage_debug_dump_ble_pairing_info(
    const struct pbl_shell *sh, const struct pbl_bt_sm_pairing_info *info);

void bluetooth_persistent_storage_debug_dump_root_keys(const struct pbl_shell *sh,
                                                       const struct pbl_bt_sm_key *irk,
                                                       const struct pbl_bt_sm_key *erk);

void bluetooth_persistent_storage_dump_contents(const struct pbl_shell *sh);
