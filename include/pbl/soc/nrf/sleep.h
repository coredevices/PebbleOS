/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Block full sleep mode, where flash is powered down and the system is woken
//! by the RTC alarm. Refcounted; balance each call with
//! soc_nrf_sleep_full_release(). Safe to call concurrently.
void soc_nrf_sleep_full_block(void);

//! Release a block taken with soc_nrf_sleep_full_block().
void soc_nrf_sleep_full_release(void);

//! Whether full sleep is currently permitted (no outstanding blocks).
bool soc_nrf_sleep_full_is_allowed(void);
