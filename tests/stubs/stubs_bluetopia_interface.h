/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef struct BTContext BTContext;

unsigned int bt_stack_id(void) {
  return 1;
}

BTContext *bluetopia_get_context(void) {
  return NULL;
}
