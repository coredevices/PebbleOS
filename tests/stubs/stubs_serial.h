/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdio.h>
#include <stdarg.h>

void dbgserial_putstr(const char* str) {
  printf("%s\n", str);
}

FORMAT_PRINTF(3, 4)
void dbgserial_putstr_fmt(char* str, unsigned int size, const char *fmt, ...) {
  va_list fmt_args;
  va_start(fmt_args, fmt);
  vprintf(fmt, fmt_args);
  va_end(fmt_args);
  printf("\n");
}

void dbgserial_putchar(uint8_t character) {
  printf("%c", character);
}

void dbgserial_putchar_lazy(uint8_t c) {
  dbgserial_putchar(c);
}
