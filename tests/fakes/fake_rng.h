/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>

static bool s_is_seeded = false;

bool rng_rand(uint32_t *rand_out) {
  if (!s_is_seeded) {
    srand(0); // Seed with constant, to make unit tests less random :)
    s_is_seeded = true;
  }
  *rand_out = rand();
  return true;
}
