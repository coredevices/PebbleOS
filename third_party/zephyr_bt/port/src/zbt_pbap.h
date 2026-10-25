/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Phone book access client on the Zephyr host; on the Zephyr thread.

#include <stdint.h>

//! Set the client up, after bt_enable().
void zbt_pbap_init(void);

//! Read @p books (bits of 1 << enum zbt_book) in @p delay_ms, joining any asked for before.
void zbt_pbap_auto(unsigned books, uint32_t delay_ms);
