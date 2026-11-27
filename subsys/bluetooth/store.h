/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/** Install NimBLE's store, reporting bond changes to the firmware. */
void pbl_bt__store_init(void);

/** Set the local IRK, before the host starts. */
void pbl_bt__store_set_local_irk(const uint8_t irk[16]);
