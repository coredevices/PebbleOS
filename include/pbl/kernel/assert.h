/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>

/**
 * @defgroup kernel_assert Assertions
 * @ingroup kernel
 * @brief Checks that raise a fatal error when an expression is false.
 *
 * Always enabled. PBL_ASSERTN() carries no message and is the cheaper of the two.
 *
 * @code{.c}
 * PBL_ASSERTN(buf != nullptr);
 * PBL_ASSERT(len <= sizeof(s_buf), "len %zu", len);
 * @endcode
 * @{
 */

/**
 * @brief Panic with a message if an expression is false.
 *
 * @param expr Expression.
 * @param fmt printf-style message; must be a string literal.
 * @param ... Message arguments.
 */
#define PBL_ASSERT(expr, fmt, ...)                                \
  do {                                                            \
    if (PBL_UNLIKELY(!(expr))) {                                  \
      PBL_PANIC_MSG("*** ASSERTION FAILED: " fmt, ##__VA_ARGS__); \
    }                                                             \
  } while (0)

/**
 * @brief Panic if an expression is false.
 *
 * @param expr Expression.
 */
#define PBL_ASSERTN(expr)        \
  do {                           \
    if (PBL_UNLIKELY(!(expr))) { \
      PBL_PANIC_BARE();          \
    }                            \
  } while (0)

/** @} */
