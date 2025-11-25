/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/order.h"

int uint32_comparator(void *a, void *b) {
  uint32_t A = *(uint32_t *)a;
  uint32_t B = *(uint32_t *)b;

  if (B > A) {
    return 1;
  } else if (B < A) {
    return -1;
  } else {
    return 0;
  }
}
