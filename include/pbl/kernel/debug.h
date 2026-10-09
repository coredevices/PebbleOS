/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/thread.h>

/**
 * @defgroup kernel_debug Introspection
 * @ingroup kernel
 * @brief Thread introspection for core dumps, fault handling, the task watchdog and telemetry.
 *
 * @code{.c}
 * static void prv_print(const struct pbl_thread_info *info, void *ctx) {
 *   if (!info->current) {
 *     printf("%s pc=%08lx\n", info->name, info->regs[PBL_THREAD_REG_PC]);
 *   }
 * }
 *
 * pbl_thread_foreach(prv_print, NULL);
 * @endcode
 * @{
 */

/** @brief Index of a register in @ref pbl_thread_info::regs; the core dump format depends on it. */
enum pbl_thread_reg {
  /** r0. */
  PBL_THREAD_REG_R0,
  /** r1. */
  PBL_THREAD_REG_R1,
  /** r2. */
  PBL_THREAD_REG_R2,
  /** r3. */
  PBL_THREAD_REG_R3,
  /** r4. */
  PBL_THREAD_REG_R4,
  /** r5. */
  PBL_THREAD_REG_R5,
  /** r6. */
  PBL_THREAD_REG_R6,
  /** r7. */
  PBL_THREAD_REG_R7,
  /** r8. */
  PBL_THREAD_REG_R8,
  /** r9. */
  PBL_THREAD_REG_R9,
  /** r10. */
  PBL_THREAD_REG_R10,
  /** r11. */
  PBL_THREAD_REG_R11,
  /** r12. */
  PBL_THREAD_REG_R12,
  /** Stack pointer, with the exception frame popped. */
  PBL_THREAD_REG_SP,
  /** Link register. */
  PBL_THREAD_REG_LR,
  /** Program counter. */
  PBL_THREAD_REG_PC,
  /** Program status register. */
  PBL_THREAD_REG_XPSR,
  /** Number of registers. */
  PBL_THREAD_REG_COUNT,
};

/** @brief Thread as seen by pbl_thread_foreach(). */
struct pbl_thread_info {
  /** Thread object. */
  struct pbl_thread *thread;
  /** Thread name. */
  const char *name;
  /** Identifier for debuggers; the address of the thread object. */
  uintptr_t id;
  /** The thread was running; its registers are live, not saved, and @ref regs is zero. */
  bool current;
  /** Saved registers, indexed by @ref pbl_thread_reg. */
  uint32_t regs[PBL_THREAD_REG_COUNT];
};

/**
 * @brief Callback of pbl_thread_foreach().
 *
 * @param info Thread being visited, valid only during the call.
 * @param ctx Context given to pbl_thread_foreach().
 */
typedef void (*pbl_thread_info_fn)(const struct pbl_thread_info *info, void *ctx);

/**
 * @brief Visit every live thread with its saved registers.
 *
 * Takes no lock; safe to call from a fault handler.
 *
 * @param fn Called once per thread.
 * @param ctx Passed to @p fn.
 */
void pbl_thread_foreach(pbl_thread_info_fn fn, void *ctx);

/** @brief Registers of a thread that is not running, read from its saved context. */
struct pbl_thread_saved_regs {
  /** Program counter. */
  uintptr_t pc;
  /** Link register. */
  uintptr_t lr;
  /** CONTROL register. */
  uint32_t control;
};

/**
 * @brief Read the saved PC, LR and CONTROL of a thread.
 *
 * Called from an ISR for the interrupted thread, it reads the live exception frame instead, so
 * the result is where that thread actually is.
 *
 * @param t Thread.
 * @param[out] regs Registers.
 */
void pbl_thread_saved_regs(const struct pbl_thread *t, struct pbl_thread_saved_regs *regs);

/** @brief Stack bounds and usage of a thread. */
struct pbl_thread_stack_info {
  /** Lowest address of the stack. */
  uintptr_t start;
  /** Stack size in bytes. */
  size_t size;
  /** Bytes at the bottom of the stack never used since the thread was created. */
  size_t high_water;
};

/**
 * @brief Get the stack bounds and high-water mark of a thread.
 *
 * Scans the stack fill pattern, so the cost grows with the unused stack.
 *
 * @param t Thread.
 * @param[out] info Stack information.
 */
void pbl_thread_stack_info(const struct pbl_thread *t, struct pbl_thread_stack_info *info);

/** @brief Run-time statistics of a thread. */
struct pbl_thread_stats {
  /** Thread object. */
  struct pbl_thread *thread;
  /** Thread name. */
  const char *name;
  /** Sequence number assigned when the thread was started. */
  uint32_t number;
  /** Time spent running, in ticks. */
  uint32_t run_time;
  /** Bytes of stack never used, see @ref pbl_thread_stack_info::high_water. */
  size_t stack_high_water;
  /** State at the time of the snapshot. */
  enum pbl_thread_state state;
};

/**
 * @brief Take a snapshot of the statistics of every live thread.
 *
 * Runs with interrupts locked.
 *
 * @param[out] out Array receiving the entries.
 * @param max Capacity of @p out; further threads are left out.
 * @param[out] total_run_time Uptime in ticks, the denominator of the run times; may be NULL.
 * @return Number of entries written.
 */
size_t pbl_thread_stats_snapshot(struct pbl_thread_stats *out, size_t max,
                                 uint32_t *total_run_time);

/**
 * @brief Count the live threads, including the idle thread.
 *
 * @return Number of threads.
 */
size_t pbl_thread_count(void);

/**
 * @brief Handle a failed kernel assertion.
 *
 * Implemented by the platform.
 *
 * @param filename Source file name of the assertion.
 * @param line Line of the assertion.
 */
[[noreturn]] void pbl_kernel_assert_failed(const char *filename, int line);

/** @} */
