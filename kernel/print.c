/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/kernel/print.h>

static pbl_print_output_t s_output;

void pbl_print_set_output(pbl_print_output_t output) {
  s_output = output;
}

void pbl_vprint(const char *fmt, va_list ap) {
  pbl_print_output_t output = s_output;
  char buffer[CONFIG_KERNEL_PRINT_BUFFER_SIZE];
  int len;

  if (output == nullptr) {
    return;
  }

  len = vsnprintf(buffer, sizeof(buffer), fmt, ap);
  if (len <= 0) {
    return;
  }

  output(buffer, (size_t)len < sizeof(buffer) ? (size_t)len : sizeof(buffer) - 1U);
}

void pbl_print(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  pbl_vprint(fmt, ap);
  va_end(ap);
}
