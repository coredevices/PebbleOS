/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/types.h>

#include <pbl_arch_cycle.h>

/**
 * @defgroup kernel_time Uptime and cycles
 * @ingroup kernel
 * @brief Monotonic time since the scheduler started, and the CPU cycle counter.
 *
 * Uptime counts kernel ticks, including those spent in tickless sleep, in 64 bits so it never
 * wraps. The cycle counter is for short, precise measurements: it is 32 bits wide, its rate
 * follows the CPU clock and it may stop while the CPU sleeps.
 *
 * @code{.c}
 * uint64_t start = pbl_uptime_ms();
 * do_work();
 * PBL_LOG_DBG("took %" PRIu32 " ms", (uint32_t)(pbl_uptime_ms() - start));
 *
 * uint32_t c0 = pbl_cycle_get_32();
 * do_short_work();
 * uint32_t cycles = pbl_cycle_get_32() - c0;
 * @endcode
 * @{
 */

/**
 * @brief Get the time since the scheduler started, in ticks.
 *
 * @return Ticks elapsed, at @ref PBL_TICK_HZ.
 */
uint64_t pbl_uptime_ticks(void);

/**
 * @brief Get the time since the scheduler started, in milliseconds.
 *
 * @return Milliseconds elapsed, with tick resolution.
 */
uint64_t pbl_uptime_ms(void);

/**
 * @brief Read the CPU cycle counter.
 *
 * Privileged only. Wraps around; compare differences, not absolute values.
 *
 * @return Current cycle count.
 */
static PBL_ALWAYS_INLINE uint32_t pbl_cycle_get_32(void) {
  return arch_cycle_get_32();
}

/**
 * @brief Get the rate of the CPU cycle counter.
 *
 * @return Cycles per second at the current CPU clock.
 */
uint32_t pbl_cycles_per_sec(void);

/** @} */
