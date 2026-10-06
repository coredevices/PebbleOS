/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include <pbl/drivers/rng.h>

bool rng_rand(uint32_t *rand_out) {
  *rand_out = arc4random();
  return true;
}
