/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

typedef struct BTContext BTContext;

PBL_WEAK unsigned int bt_stack_id(void) {
  return 1;
}

PBL_WEAK BTContext *bluetopia_get_context(void) {
  return nullptr;
}
