/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdarg.h>
#include_next <stdio.h>

inline static int sniprintf(char * restrict str, size_t size, const char * restrict format, ...) {
  va_list ap;
  va_start(ap, format);
  int result = vsnprintf(str, size, format, ap);
  va_end(ap);

  return result;
}
