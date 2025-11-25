/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "task_init.h"

#include "drivers/rng.h"
#include "drivers/rtc.h"

#include <stdlib.h>

void task_init(void) {
  uint32_t seed;
  if (!rng_rand(&seed)) {
    // Fallback, time XOR'd with an approximation of the current stack pointer:
    seed = rtc_get_time() ^ (uintptr_t) &seed;
  }
  srand(seed);
}
