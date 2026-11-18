/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/sm_types.h>

/** Install NimBLE's store, reporting bond changes to the firmware. */
void pbl_bt__store_init(void);

/** Set the local IRK, before the host starts. */
void pbl_bt__store_set_local_irk(const struct pbl_bt_sm_key *irk);
