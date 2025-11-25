/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/dbgserial.h"
#include "system/logging.h"
#include "system/passert.h"

#include "util/assert.h"
#include "util/logging.h"

void util_log(const char *filename, int line, const char *string) {
  pbl_log(LOG_LEVEL_INFO, filename, line, string);
}

void util_dbgserial_str(const char *string) {
  dbgserial_putstr(string);
}

NORETURN util_assertion_failed(const char *filename, int line) {
  passert_failed_no_message(filename, line);
}
