/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup soc_st STM32
 * @ingroup soc
 * @brief ST STM32 SoC interfaces.
 *
 * A driver whose peripheral does not work in an idle state blocks it while busy:
 *
 * @code{.c}
 * pbl_idle_state_block(SOC_STM32_IDLE_STOP); // the DMA2D clock stops in Stop mode
 * prv_dma2d_run();
 * pbl_idle_state_release(SOC_STM32_IDLE_STOP);
 * @endcode
 * @{
 */

/** @brief Idle states, indices in @ref pbl_soc_idle_states. */
enum soc_stm32_idle_state {
  /** Sleep: the CPU clock stops, the peripherals run. */
  SOC_STM32_IDLE_WFI,
  /**
   * Stop: every clock but the LSE and LSI stops, the memories and registers are kept. Only
   * offered when the system timer runs on the LSE.
   */
  SOC_STM32_IDLE_STOP,
};

/** @} */
