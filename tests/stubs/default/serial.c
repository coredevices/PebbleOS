/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/compiler.h>

PBL_WEAK void dbgserial_putstr(const char *str) {
  printf("%s\n", str);
}

PBL_WEAK PBL_FORMAT_PRINTF(3, 4) void dbgserial_putstr_fmt(char *str, unsigned int size,
                                                           const char *fmt, ...) {
  va_list fmt_args;
  va_start(fmt_args, fmt);
  vprintf(fmt, fmt_args);
  va_end(fmt_args);
  printf("\n");
}

PBL_WEAK void dbgserial_putchar(uint8_t character) {
  printf("%c", character);
}

PBL_WEAK void dbgserial_putchar_lazy(uint8_t c) {
  dbgserial_putchar(c);
}
