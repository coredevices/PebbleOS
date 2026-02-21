/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "system/logging.h"
#include "system/passert.h"

void __assert_func(const char *file, int line, const char *func, const char *e) {
  PBL_LOG_ERR("assert in %s:%d", file, line);
  PBL_LOG_ERR("%s, expr: %s", func, e);
  PBL_ASSERT(0, "libc assert()");
}
