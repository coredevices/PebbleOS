/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup soc_nrf nRF
 * @ingroup soc
 * @brief Nordic nRF52 SoC interfaces.
 *
 * When idle for long enough, the nRF52 enters full sleep: flash is powered down, the tick is
 * paused and the CPU is woken by the RTC alarm. Code that cannot tolerate that, e.g. while a
 * transfer depends on the tick or on flash, blocks it for the duration:
 *
 * @code{.c}
 * soc_nrf_sleep_full_block();
 * prv_run_transfer();
 * soc_nrf_sleep_full_release();
 * @endcode
 * @{
 */

/**
 * @brief Block full sleep.
 *
 * Plain WFI sleep remains allowed. Refcounted; balance each call with
 * soc_nrf_sleep_full_release(). Safe to call concurrently, including from ISRs.
 */
void soc_nrf_sleep_full_block(void);

/** @brief Release a block taken with soc_nrf_sleep_full_block(). */
void soc_nrf_sleep_full_release(void);

/**
 * @brief Check whether full sleep is permitted.
 *
 * @return true if no block is outstanding.
 */
bool soc_nrf_sleep_full_is_allowed(void);

/** @} */
