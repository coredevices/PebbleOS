/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_bootbits.h"

#include <inttypes.h>
#include <stdbool.h>

static uint32_t s_bootbits = 0;

void boot_bit_clear(BootBitValue bit) {
  s_bootbits &= ~bit;
}

bool boot_bit_test(BootBitValue bit) {
  return (s_bootbits & bit);
}

void fake_boot_bit_set(BootBitValue bit) {
  s_bootbits |= bit;
}
