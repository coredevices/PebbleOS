/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/types.h"

/**
 * @defgroup kernel_idle Idle and tick
 * @ingroup kernel
 * @brief Interface between the kernel and the SoC's tickless idle code.
 *
 * The SoC implements pbl_soc_idle() and pbl_soc_tick_enable(); the kernel provides the rest.
 * A typical pbl_soc_idle():
 *
 * @code{.c}
 * void pbl_soc_idle(pbl_tick_t max_ticks) {
 *   __disable_irq();
 *   if (pbl_idle_confirm()) {
 *     prv_timer_stop_tick();
 *     prv_sleep_until(max_ticks);
 *     pbl_idle_slept(prv_ticks_elapsed());
 *     prv_timer_start_tick();
 *   }
 *   __enable_irq();
 * }
 * @endcode
 * @{
 */

/**
 * @brief Idle the CPU.
 *
 * Implemented by the SoC. Runs on the idle thread when nothing is runnable, only for gaps of at
 * least two ticks; may sleep. Once interrupts are masked it must check pbl_idle_confirm() before
 * sleeping, and report through pbl_idle_slept() any ticks that passed while the tick interrupt
 * was off.
 *
 * @param max_ticks Ticks until the next timeout, @ref PBL_TICK_FOREVER if none.
 */
void pbl_soc_idle(pbl_tick_t max_ticks);

/**
 * @brief Set up the tick interrupt.
 *
 * Implemented by the SoC; called when the scheduler starts. A SoC that drives the tick itself
 * calls pbl_kernel_tick_isr() from its handler.
 *
 * @return true if the SoC set up the tick, false to let the kernel program SysTick itself.
 */
bool pbl_soc_tick_enable(void);

/**
 * @brief Confirm that the CPU may still go to sleep.
 *
 * Call with interrupts masked.
 *
 * @return false if something became runnable since the idle thread decided to sleep.
 */
bool pbl_idle_confirm(void);

/**
 * @brief Account for a sleep during which no tick interrupt ran.
 *
 * Advances the tick count and expires the timeouts that fell due.
 *
 * @param elapsed Ticks slept.
 */
void pbl_idle_slept(pbl_tick_t elapsed);

/** @brief Tick interrupt body, for SoCs that own the SysTick handler. */
void pbl_kernel_tick_isr(void);

/** @} */
