/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/math.h"

#include <stdint.h>
#include <stdbool.h>

int ceil_log_two(uint32_t n) {
    // clz stands for Count Leading Zeroes. We use it to find the MSB
    int msb = 31 - __builtin_clz(n);
    // popcount counts the number of set bits in a word (1's)
    bool power_of_two = __builtin_popcount(n) == 1;
    // if not exact power of two, use the next power of two
    // we want to err on the side of caution and want to
    // always round up
    return ((power_of_two) ? msb : (msb + 1));
}
