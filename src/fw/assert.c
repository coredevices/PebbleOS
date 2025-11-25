/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/logging.h"
#include "system/passert.h"

void __assert_func(const char *file, int line, const char *func, const char *e) {
  PBL_LOG(LOG_LEVEL_ERROR, "assert at line %d, func: %s - %s", line, func, e);
  WTF;
}
