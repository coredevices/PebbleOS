/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "util/likely.h"

#include <stdint.h>

NORETURN os_assertion_failed(const char *filename, int line);
NORETURN os_assertion_failed_lr(const char *filename, int line, uint32_t lr);

#define OS_ASSERT(expr) \
  do { \
    if (UNLIKELY(!(expr))) { \
      os_assertion_failed(__FILE_NAME__, __LINE__); \
    } \
  } while (0)

#define OS_ASSERT_LR(expr, lr) \
  do { \
    if (UNLIKELY(!(expr))) { \
      os_assertion_failed_lr(__FILE_NAME__, __LINE__, lr); \
    } \
  } while (0)
