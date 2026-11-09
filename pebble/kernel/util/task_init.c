/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "task_init.h"

#include <stdlib.h>

#include <pbl/drivers/rng.h>
#include <pbl/kernel/time.h>

void task_init(void) {
  uint32_t seed;
  if (!rng_rand(&seed)) {
    // Fallback, time XOR'd with an approximation of the current stack pointer:
    seed = pbl_realtime() ^ (uintptr_t)&seed;
  }
  srand(seed);
}
