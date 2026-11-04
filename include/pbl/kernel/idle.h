/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/types.h>

/**
 * @defgroup kernel_idle Idle states
 * @ingroup kernel
 * @brief Low-power states the CPU idles in, provided by the SoC.
 *
 * When nothing is runnable, the kernel picks the deepest state that is allowed now and worth
 * entering for the time until the next timeout, programs the system clock event device to wake
 * the CPU up early enough, enters the state, and accounts for the time spent in it from the
 * system clock source. The SoC only puts the CPU to sleep.
 *
 * @code{.c}
 * static void prv_wfi(void) {
 *   __DSB();
 *   __WFI();
 *   __ISB();
 * }
 *
 * const struct pbl_idle_state pbl_soc_idle_states[] = {
 *   {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = prv_wfi},
 * };
 * const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);
 *
 * int pbl_soc_idle_state_limit(void) {
 *   return 0;
 * }
 * @endcode
 * @{
 */

/** @brief Maximum number of idle states. */
#define PBL_IDLE_STATES_MAX 4

/** @brief Idle state. */
struct pbl_idle_state {
  /** Name, for statistics. */
  const char *name;
  /** Shortest time until the next timeout worth entering the state for, in ticks. */
  pbl_tick_t min_ticks;
  /** Time the state takes to wake up from, in ticks; the CPU is woken up this early. */
  pbl_tick_t exit_ticks;
  /**
   * Enter the state. Called with every interrupt masked; returns once one is pending, with
   * interrupts still masked.
   */
  void (*enter)(void);
};

/** @brief Idle states, shallowest first. Defined by the SoC. */
extern const struct pbl_idle_state pbl_soc_idle_states[];

/** @brief Number of entries in @ref pbl_soc_idle_states. Defined by the SoC. */
extern const size_t pbl_soc_num_idle_states;

/**
 * @brief Get the deepest idle state allowed now.
 *
 * Implemented by the SoC. Called with every interrupt masked.
 *
 * @return Index in @ref pbl_soc_idle_states, or -1 to keep the CPU running.
 */
int pbl_soc_idle_state_limit(void);

/**
 * @brief Get the time spent in an idle state since boot.
 *
 * @param state Index in @ref pbl_soc_idle_states.
 * @return Microseconds.
 */
uint64_t pbl_idle_residency_us(size_t state);

/** @} */
