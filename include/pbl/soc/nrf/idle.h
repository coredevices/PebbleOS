/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup soc_nrf nRF
 * @ingroup soc
 * @brief Nordic nRF52 SoC interfaces.
 *
 * When idle for long enough, the nRF52 enters full sleep, with the flash powered down. Code that
 * cannot tolerate that, e.g. while a transfer depends on the flash, blocks it for the duration:
 *
 * @code{.c}
 * pbl_idle_state_block(SOC_NRF_IDLE_FULL);
 * prv_run_transfer();
 * pbl_idle_state_release(SOC_NRF_IDLE_FULL);
 * @endcode
 * @{
 */

/** @brief Idle states, see pbl_idle_state_block(). */
enum soc_nrf_idle_state {
  /** WFI. */
  SOC_NRF_IDLE_WFI,
  /** WFI with the flash powered down. */
  SOC_NRF_IDLE_FULL,
};

/** @} */
