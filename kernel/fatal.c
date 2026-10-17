/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>
#include <pbl/logging/logging.h>

#define CORE_NUMBER 0

[[noreturn]] static void prv_panic_vargs(const char *filename, int line, uintptr_t lr,
                                         const char *fmt, va_list args) {
  char buffer[160];

  vsniprintf(buffer, sizeof(buffer), fmt, args);
  pbl_log_sync(LOG_LEVEL_ALWAYS, filename, line, "%s", buffer);

  pbl_fatal_error_handler(lr);
}

[[noreturn]] void pbl_panic_at(const char *filename, int line, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  prv_panic_vargs(filename, line, (uintptr_t)PBL_RETURN_ADDRESS(0), fmt, args);
}

[[noreturn]] void pbl_panic_bare_at(const char *filename, int line) {
  pbl_log_sync(LOG_LEVEL_ALWAYS, filename, line, "*** PANIC");

  pbl_fatal_error_handler((uintptr_t)PBL_RETURN_ADDRESS(0));
}

[[noreturn]] void pbl_panic_hashed(uint32_t packed_loghash, ...) {
  uintptr_t lr = (uintptr_t)PBL_RETURN_ADDRESS(0);
  PBL_LOG_ALWAYS("PANIC at LR 0x%" PRIxPTR, lr);

  va_list args;
  va_start(args, packed_loghash);
  pbl_log_hashed_vargs(false, CORE_NUMBER, packed_loghash, args);
  va_end(args);

  pbl_fatal_error_handler(lr);
}

[[noreturn]] void pbl_panic_bare(void) {
  uintptr_t lr = (uintptr_t)PBL_RETURN_ADDRESS(0);
  PBL_LOG_ALWAYS("PANIC at LR 0x%" PRIxPTR, lr);

  pbl_fatal_error_handler(lr);
}
