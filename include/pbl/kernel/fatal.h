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
 * pbl_fatal_error_handler(), which the platform implements.
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
 * Implemented by the platform; the kernel calls it once the error is logged. It must not return,
 * e.g. it saves a core dump and reboots.
 *
 * @param lr Address of the code that raised the error.
 */
[[noreturn]] void pbl_fatal_error_handler(uintptr_t lr);

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
