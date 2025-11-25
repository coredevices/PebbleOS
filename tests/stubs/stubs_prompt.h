/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <stdio.h>

void prompt_command_continues_after_returning(void) {
}

void prompt_command_finish(void) {
}

void prompt_send_response(const char* response) {
  printf("%s\n", response);
}

void prompt_send_response_fmt(char* buffer, size_t buffer_size, const char* fmt, ...) {
  va_list fmt_args;
  va_start(fmt_args, fmt);
  vprintf(fmt, fmt_args);
  va_end(fmt_args);
  printf("\n");
}
