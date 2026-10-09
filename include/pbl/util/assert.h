/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_assert Library assertions
 * @ingroup util
 * @brief Assertions of the utility library.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#ifndef __FILE_NAME__
#ifdef __FILE_NAME_LEGACY__
#define __FILE_NAME__ __FILE_NAME_LEGACY__
#else
#define __FILE_NAME__ __FILE__
#endif
#endif
/** @endcond */

/**
 * @brief Handle a failed UTIL_ASSERT().
 *
 * The library provides a weak implementation that logs and exits; the firmware overrides it to
 * crash with a core dump.
 *
 * @param filename Source file name.
 * @param line Source line number.
 */
[[noreturn]] void util_assertion_failed(const char *filename, int line);

/**
 * @brief Assert that an expression is true.
 *
 * Always enabled.
 *
 * @param expr Expression.
 */
#define UTIL_ASSERT(expr)                             \
  do {                                                \
    if (PBL_UNLIKELY(!(expr))) {                      \
      util_assertion_failed(__FILE_NAME__, __LINE__); \
    }                                                 \
  } while (0)

/** @} */
