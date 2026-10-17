/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>

#include <clar_asserts.h>

PBL_WEAK [[noreturn]] void pbl_panic_at(const char *filename, int line_number, const char *message,
                                        ...) {
  if (clar_expecting_panic) {
    clar_panic_occurred = true;
    longjmp(clar_panic_jmp_buf, 1);
  } else {
    va_list ap;
    va_start(ap, message);
    printf("*** ASSERTION FAILED: %s:%u\n", filename, line_number);
    if (message) {
      vprintf(message, ap);
    }
    va_end(ap);
    printf("\n");

    cl_fail(message);
  }
  while (1)
    ;
}

PBL_WEAK [[noreturn]] void util_assertion_failed(const char *filename, int line) {
  pbl_panic_at(filename, line, nullptr);
}

PBL_WEAK [[noreturn]] void pbl_panic_bare_at(const char *filename, int line_number) {
  pbl_panic_at(filename, line_number, nullptr);
}
