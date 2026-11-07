/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/coredump/format.h>
#include <pbl/kernel/debug.h>

/**
 * @defgroup coredump_arch Architecture interface
 * @ingroup coredump
 * @brief What an architecture implements to support core dumps.
 *
 * An architecture selects @c ARCH_HAS_COREDUMP and implements these functions. Except for
 * arch_coredump_trigger(), they are called from pbl_coredump_arch_handler().
 * @{
 */

/**
 * @brief Stop the system and enter pbl_coredump_arch_handler().
 *
 * Returns only if that failed.
 */
void arch_coredump_trigger(void);

/**
 * @brief Get the registers of the thread that was running.
 *
 * If the thread faulted, these are its registers at the fault, not those of the fault handler.
 *
 * @param[out] regs Registers, indexed by @ref pbl_thread_reg.
 */
void arch_coredump_thread_regs(uint32_t regs[PBL_THREAD_REG_COUNT]);

/**
 * @brief Get the registers of the exception that was running, if any.
 *
 * If it faulted, these are its registers at the fault, not those of the fault handler.
 *
 * @param[out] regs Registers, indexed by @ref pbl_thread_reg.
 * @return true if an exception was running.
 */
bool arch_coredump_exception_regs(uint32_t regs[PBL_THREAD_REG_COUNT]);

/** @brief Get the special registers at the time of the dump. */
const struct pbl_coredump_extra_regs *arch_coredump_extra_regs(void);

/**
 * @brief Write the core dump and reset.
 *
 * Called by the architecture once its state is captured, with OS services unavailable.
 */
[[noreturn]] void pbl_coredump_arch_handler(void);

/** @} */
