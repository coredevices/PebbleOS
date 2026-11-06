/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! Fuel gauge state kept across reboots: in the MFG_BATTERY_STATE flash region
//! on manufacturing firmware, in a settings file otherwise.

int battery_state_store_erase(void);

//! @return 0 on success, -ENOENT if no state of this size was saved, or a
//! negative errno on failure.
int battery_state_store_load(void *state, size_t size);

int battery_state_store_save(const void *state, size_t size);
