/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/math.h"

#include <stdint.h>
#include <stdbool.h>

int32_t sign_extend(uint32_t a, int bits) {
  if (bits == 32) {
    return a;
  }

  // http://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend
  int const m = 1U << (bits - 1); // mask can be pre-computed if b is fixed

  a = a & ((1U << bits) - 1);  // (Skip this if bits in x above position b are already zero.)
  return (a ^ m) - m;
}

int32_t serial_distance32(uint32_t a, uint32_t b) {
  return serial_distance(a, b, 32);
}

int32_t serial_distance(uint32_t a, uint32_t b, int bits) {
  // See https://en.wikipedia.org/wiki/Serial_Number_Arithmetic
  const int64_t a_minus_b = a - b;
  const int64_t b_minus_a = b - a;
  const bool a_is_earlier_than_b = (a < b && b_minus_a < (1 << (bits - 1))) || (a > b && a_minus_b > (1 << (bits - 1)));
  return sign_extend(a_is_earlier_than_b ? -a_minus_b : b_minus_a, bits);
}

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

//! newton's method for floor(sqrt(x)) -> should always converge
int32_t integer_sqrt(int64_t x) {
  if (x < 0) {
    return 0;
  }
  int64_t last_res = 0x3fff;
  uint16_t iterations = 0;
  while ((last_res > 0) && (iterations < 15)) {
    last_res = ((x / last_res) + last_res)/2;
    iterations++;
  }
  return (last_res);
}

uint32_t next_exponential_backoff(uint32_t *attempt, uint32_t initial_value, uint32_t max_value) {
  if (*attempt > 31) {
    return max_value;
  }
  uint32_t backoff_multiplier = 0x1 << (*attempt)++;
  uint32_t next_value = initial_value * backoff_multiplier;
  return MIN(next_value, max_value);
}

uint32_t gcd(uint32_t a, uint32_t b) {
  // Infinite loops are bad
  if (a == 0 || b == 0) {
    return 0;
  }

  // Euclidean algorithm, yay!
  while (a != b) {
    if (a > b) {
      a -= b;
    } else {
      b -= a;
    }
  }
  return a;
}
