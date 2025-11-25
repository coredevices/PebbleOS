/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

static inline void swap16(int16_t *a, int16_t *b) {
  int16_t t = *a;
  *a = *b;
  *b = t;
}
