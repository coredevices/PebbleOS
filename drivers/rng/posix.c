/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stddef.h>

#include <pbl/drivers/rng.h>

#include <sys/random.h>

int rng_rand(uint32_t *rand_out) {
  if (getentropy(rand_out, sizeof(*rand_out)) != 0) {
    return -errno;
  }

  return 0;
}
