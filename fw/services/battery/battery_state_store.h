/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

//! Fuel gauge state kept across reboots: in the MFG_BATTERY_STATE flash region
//! on manufacturing firmware, in a settings file otherwise.

void battery_state_store_erase(void);

//! Returns false if no state of this size was saved.
bool battery_state_store_load(void *state, size_t size);

void battery_state_store_save(const void *state, size_t size);
