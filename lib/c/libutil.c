/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/fatal.h>
#include <pbl/logging/logging.h>
#include <pbl/util/assert.h>
#include <pbl/util/logging.h>

void util_log(const char *filename, int line, const char *string) {
#ifdef CONFIG_LOG
  pbl_log(LOG_LEVEL_INFO, filename, line, "%s", string);
#endif
}

[[noreturn]] void util_assertion_failed(const char *filename, int line) {
  pbl_panic_bare_at(filename, line);
}
