/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdlib.h>

// Implemented in fw/util/rand/rand.c
//! rand() without the 31-bit truncation
uint32_t rand32(void);

static inline int bounded_rand_int(int M, int N) {
  return M + rand() / (RAND_MAX / (N - M + 1) + 1);
}
