/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/attributes.h"

#include <stdint.h>


// Override libgcc's table-driven __builtin_popcount implementation
#ifdef __arm__
EXTERNALLY_VISIBLE int __popcountsi2(uint32_t val) {
  // Adapted from http://www.sciencezero.org/index.php?title=ARM%3a_Count_ones_%28bit_count%29
  uint32_t tmp;
  __asm("and  %[tmp], %[val], #0xaaaaaaaa\n\t"
        "sub  %[val], %[val], %[tmp], lsr #1\n\t"

        "and  %[tmp], %[val], #0xcccccccc\n\t"
        "and  %[val], %[val], #0x33333333\n\t"
        "add  %[val], %[val], %[tmp], lsr #2\n\t"

        "add  %[val], %[val], %[val], lsr #4\n\t"
        "and  %[val], %[val], #0x0f0f0f0f\n\t"

        "add  %[val], %[val], %[val], lsr #8\n\t"
        "add  %[val], %[val], %[val], lsr #16\n\t"
        "and  %[val], %[val], #63\n\t"
        : [val] "+l" (val), [tmp] "=&l" (tmp));
  return val;
}
#endif  // __arm__
