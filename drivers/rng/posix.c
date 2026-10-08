/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <sys/random.h>

#include <pbl/drivers/rng.h>

bool rng_rand(uint32_t *rand_out) {
  return getentropy(rand_out, sizeof(*rand_out)) == 0;
}
