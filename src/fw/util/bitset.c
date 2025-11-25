/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bitset.h"

#include <stdint.h>

uint8_t count_bits_set(uint8_t *bitset_bytes, int num_bits) {
  uint8_t num_bits_set = 0;
  int num_bytes = (num_bits + 7) / 8;
  if ((num_bits % 8) != 0) {
    bitset_bytes[num_bytes] &= ((0x1 << (num_bits)) - 1);
  }

  for (int i = 0; i < num_bytes; i++) {
    num_bits_set += __builtin_popcount(bitset_bytes[i]);
  }

  return (num_bits_set);
}
