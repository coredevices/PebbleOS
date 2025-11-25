/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <setjmp.h>

bool clar_expecting_passert = false;
bool clar_passert_occurred = false;
jmp_buf clar_passert_jmp_buf;

void
clar__assert(
  int condition,
  const char *file,
  int line,
  const char *error_msg,
  const char *description,
  int should_abort)
{};
