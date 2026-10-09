/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/types.h>

/**
 * @defgroup kernel_sched Scheduler
 * @ingroup kernel
 * @brief Scheduler start, preemption control and uptime.
 *
 * pbl_sched_lock() keeps the calling thread running without masking interrupts: switches
 * requested meanwhile, e.g. by an ISR waking a higher priority thread, happen at the outermost
 * pbl_sched_unlock(). Blocking while the scheduler is locked is not allowed.
 *
 * @code{.c}
 * pbl_sched_lock();
 * prv_update_shared_state();
 * pbl_sched_unlock();
 * @endcode
 * @{
 */

/**
 * @brief Start the scheduler.
 *
 * Creates the idle thread and switches to the highest priority thread. Called once from main(),
 * after the first threads have been created.
 */
[[noreturn]] void pbl_kernel_start(void);

/**
 * @brief Check whether the scheduler has started.
 *
 * @return true once pbl_kernel_start() has run.
 */
bool pbl_kernel_is_started(void);

/**
 * @brief Check whether threads are being scheduled.
 *
 * @return true if the scheduler has started and is not locked.
 */
bool pbl_kernel_is_running(void);

/**
 * @brief Disable preemption.
 *
 * ISRs still run. Nestable; balance each call with pbl_sched_unlock().
 */
void pbl_sched_lock(void);
/** @brief Undo one pbl_sched_lock(), performing any deferred switch at the outermost one. */
void pbl_sched_unlock(void);
/**
 * @brief Check whether preemption is disabled.
 *
 * @return true while pbl_sched_lock() is in effect.
 */
bool pbl_sched_is_locked(void);

/**
 * @brief Get the time since the scheduler started.
 *
 * @return Ticks elapsed, including time spent in tickless sleep. Wraps around.
 */
pbl_tick_t pbl_uptime_ticks(void);

/** @} */
