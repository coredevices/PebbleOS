/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup soc_sf32lb SF32LB
 * @ingroup soc
 * @brief SiFli SF32LB SoC interfaces.
 *
 * A driver whose peripheral does not work in an idle state blocks it while busy:
 *
 * @code{.c}
 * pbl_idle_state_block(SOC_SF32LB_IDLE_DEEPSLEEP); // the PWM does not run in deep sleep
 * prv_pwm_start();
 * prv_pwm_wait_done();
 * prv_pwm_stop();
 * pbl_idle_state_release(SOC_SF32LB_IDLE_DEEPSLEEP);
 * @endcode
 * @{
 */

/** @brief Idle states, see pbl_idle_state_block(). */
enum soc_sf32lb_idle_state {
  /** Light WFI. */
  SOC_SF32LB_IDLE_WFI,
  /** Deep WFI. */
  SOC_SF32LB_IDLE_DEEPWFI,
  /** Deep sleep. */
  SOC_SF32LB_IDLE_DEEPSLEEP,
};

/** @} */
