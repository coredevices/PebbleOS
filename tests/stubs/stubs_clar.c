/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <setjmp.h>

bool clar_expecting_panic = false;
bool clar_panic_occurred = false;
jmp_buf clar_panic_jmp_buf;

void clar__assert(int condition, const char *file, int line, const char *error_msg,
                  const char *description, int should_abort) {};
