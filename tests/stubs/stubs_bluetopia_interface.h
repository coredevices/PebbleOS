/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>  // For NULL

typedef struct BTContext BTContext;

static unsigned int bt_stack_id(void) {
  return 1;
}

static BTContext *bluetopia_get_context(void) {
  return NULL;
}
