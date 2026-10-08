/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/logging/logging.h>
#include <pbl/util/assert.h>
#include <pbl/util/logging.h>

#include <console/dbgserial.h>
#include <system/passert.h>

void util_log(const char *filename, int line, const char *string) {
  pbl_log(LOG_LEVEL_INFO, filename, line, "%s", string);
}

void util_dbgserial_str(const char *string) {
  dbgserial_putstr(string);
}

PBL_NORETURN void util_assertion_failed(const char *filename, int line) {
  passert_failed_no_message(filename, line);
}
