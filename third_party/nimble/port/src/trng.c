/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <trng/trng.h>

#include <pbl/drivers/rng.h>
#include <system/passert.h>

#include <string.h>

struct trng_dev {
  uint8_t unused;
};

static struct trng_dev s_trng;

void *os_dev_open(const char *devname, uint32_t timo, void *arg) {
  PBL_ASSERTN(strcmp(devname, "trng") == 0);

  return &s_trng;
}

size_t trng_read(struct trng_dev *trng, void *ptr, size_t size) {
  uint8_t *out = ptr;
  size_t left = size;

  while (left > 0U) {
    uint32_t rand;
    size_t n = left < sizeof(rand) ? left : sizeof(rand);

    PBL_ASSERTN(rng_rand(&rand));
    memcpy(out, &rand, n);
    out += n;
    left -= n;
  }

  return size;
}
