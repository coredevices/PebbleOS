/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/compiler.h>

#include <clar_asserts.h>

PBL_WEAK [[noreturn]] void passert_failed(const char *filename, int line_number,
                                          const char *message, ...) {
  if (clar_expecting_passert) {
    clar_passert_occurred = true;
    longjmp(clar_passert_jmp_buf, 1);
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
  passert_failed(filename, line, nullptr);
}

PBL_WEAK [[noreturn]] void passert_failed_no_message(const char *filename, int line_number) {
  passert_failed(filename, line_number, nullptr);
}

PBL_WEAK [[noreturn]] void passert_failed_no_message_with_lr(const char *filename, int line_number,
                                                             uint32_t lr) {
  passert_failed(filename, line_number, nullptr);
}

PBL_WEAK void croak(const char *filename, int line_number, const char *fmt, ...) {
  cl_fail(fmt);
  while (1)
    ;
}

typedef struct Heap Heap;

PBL_WEAK [[noreturn]] void croak_oom(size_t bytes, int saved_lr, Heap *heap_ptr) {
  cl_fail("CROAK OOM");
  while (1)
    ;
}

PBL_WEAK [[noreturn]] void wtf(void) {
  cl_fail("WTF");
  while (1)
    ;
}
