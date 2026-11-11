/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

#ifdef CONFIG_LOG_HASHED
#include <pbl/logging/log_hashing.h>
#endif

/**
 * @defgroup kernel_fatal Fatal errors
 * @ingroup kernel
 * @brief Stop the system on an unrecoverable error.
 *
 * The kernel logs the error with the address of the code that raised it, then hands over to
 * pbl_fatal_error_handler(), which an application may override. CPU faults go to
 * pbl_fault_fatal_handler(), or pbl_fault_thread_handler() when only an unprivileged thread
 * needs to stop.
 *
 * @code{.c}
 * switch (state) {
 *   case STATE_IDLE:
 *     break;
 *   case STATE_BUSY:
 *     pbl_panic("busy since %u", s_busy_since);
 *   default:
 *     pbl_panic();
 * }
 * @endcode
 * @{
 */

/**
 * @brief Handle a fatal error.
 *
 * The kernel calls it once the error is logged. The default flushes the logs and resets; an
 * application overrides it, e.g. to save a core dump first. It must not return.
 *
 * @param lr Address of the code that raised the error.
 */
[[noreturn]] void pbl_fatal_error_handler(uintptr_t lr);

/** @brief Cause of a CPU fault. */
enum pbl_fault_reason {
  /** A thread ran out of stack. */
  PBL_FAULT_STACK_OVERFLOW,
  /** Memory access the MPU forbids. */
  PBL_FAULT_MEMORY,
  /** Bus error. */
  PBL_FAULT_BUS,
  /** Undefined instruction, bad state, division by zero... */
  PBL_FAULT_USAGE,
  /** Fault while handling a fault or an exception. */
  PBL_FAULT_HARD,
};

/** @brief CPU fault, as handed to the fault handlers. */
struct pbl_fault_info {
  /** Cause. */
  enum pbl_fault_reason reason;
  /** Address of the faulting instruction; 0 if unknown. */
  uintptr_t pc;
  /** Return address of the faulting code; 0 if unknown. */
  uintptr_t lr;
  /** Data address the fault happened on; 0 if unknown. */
  uintptr_t addr;
};

/**
 * @brief Record a CPU fault the system can't recover from.
 *
 * The kernel calls it from the fault exception before logging anything, so the record survives a
 * fault or hang while logging. The default does nothing; an application overrides it, e.g. to
 * record why the system is about to reset. It must not log.
 *
 * @param info Fault.
 */
void pbl_fault_fatal_record(const struct pbl_fault_info *info);

/**
 * @brief Handle a CPU fault the system can't recover from.
 *
 * The kernel calls it from the fault exception once the fault is recorded and logged. The default
 * flushes the logs and resets; an application overrides it, e.g. to save a core dump. It must not
 * return.
 *
 * @param info Fault.
 */
[[noreturn]] void pbl_fault_fatal_handler(const struct pbl_fault_info *info);

/**
 * @brief Handle a CPU fault in an unprivileged thread.
 *
 * The kernel logs the fault and leaves the faulting code for this function, on the same thread,
 * now privileged. The default is pbl_fault_fatal_handler(); an application overrides it, e.g. to
 * kill only that thread. It must not return.
 *
 * @param info Fault.
 */
[[noreturn]] void pbl_fault_thread_handler(const struct pbl_fault_info *info);

/** @cond INTERNAL */
[[noreturn]] void pbl_panic_hashed(uint32_t packed_loghash, ...);
[[noreturn]] void pbl_panic_at(const char *filename, int line, const char *fmt, ...)
    PBL_FORMAT_PRINTF(3, 4);
[[noreturn]] void pbl_panic_bare(void);
[[noreturn]] void pbl_panic_bare_at(const char *filename, int line);

#ifdef CONFIG_LOG_HASHED
#define PBL_PANIC_MSG(fmt, ...) \
  NEW_LOG_HASH(pbl_panic_hashed, LOG_LEVEL_ALWAYS, LOG_COLOR_RED, fmt, ##__VA_ARGS__)
#define PBL_PANIC_BARE() pbl_panic_bare()
#else
#define PBL_PANIC_MSG(fmt, ...) pbl_panic_at(__FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)
#define PBL_PANIC_BARE()        pbl_panic_bare_at(__FILE_NAME__, __LINE__)
#endif

#define PBL_PANIC_SELECT(msg) PBL_PANIC_##msg
#define PBL_PANIC_(...)       PBL_PANIC_BARE()
#define PBL_PANIC_MESSAGE(fmt, ...)                  \
  do {                                               \
    PBL_PANIC_MSG("*** PANIC: " fmt, ##__VA_ARGS__); \
  } while (0)
/** @endcond */

/**
 * @brief Stop the system.
 *
 * Without arguments only the address of the caller is logged, the cheapest form, and the call
 * is an expression.
 *
 * @param ... Optional printf-style message, a string literal, and its arguments.
 */
#define pbl_panic(...) PBL_PANIC_SELECT(__VA_OPT__(MESSAGE))(__VA_ARGS__)

/** @} */
