/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"
#include "util/likely.h"

#ifndef __FILE_NAME__
#ifdef __FILE_NAME_LEGACY__
#define __FILE_NAME__ __FILE_NAME_LEGACY__
#else
#define __FILE_NAME__ __FILE__
#endif
#endif

NORETURN util_assertion_failed(const char *filename, int line);

#define UTIL_ASSERT(expr) \
  do { \
    if (UNLIKELY(!(expr))) { \
      util_assertion_failed(__FILE_NAME__, __LINE__); \
    } \
  } while (0)
