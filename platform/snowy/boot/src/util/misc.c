/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "misc.h"
#include "system/logging.h"
#include "system/passert.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

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

void itoa(uint32_t num, char *buffer, int buffer_length) {
  if (buffer_length < 11) {
    PBL_LOG(LOG_LEVEL_WARNING, "ito buffer too small");
    return;
  }
  *buffer++ = '0';
  *buffer++ = 'x';

  for (int i = 7; i >= 0; --i) {
    uint32_t digit = (num & (0xf << (i * 4))) >> (i * 4);

    char c;
    if (digit < 0xa) {
      c = '0' + digit;
    } else if (digit < 0x10) {
      c = 'a' + (digit - 0xa);
    } else {
      c = ' ';
    }

    *buffer++ = c;
  }
  *buffer = '\0';
}

static int8_t ascii_hex_to_int(const uint8_t c) {
  if (isdigit(c)) return c - '0';
  if (isupper(c)) return (c - 'A') + 10;
  if (islower(c)) return (c - 'a') + 10;

  return -1;
}

static uint8_t ascii_hex_to_uint(const uint8_t msb, const uint8_t lsb) {
  return 16 * ascii_hex_to_int(msb) + ascii_hex_to_int(lsb);
}

uintptr_t str_to_address(const char *address_str) {
  unsigned int address_str_length = strlen(address_str);

  if (address_str_length < 3) return -1; // Invalid address, must be at least 0x[0-9a-f]+
  if (address_str[0] != '0' || address_str[1] != 'x') return -1; // Incorrect address prefix.

  address_str += 2;

  uintptr_t address = 0;
  for (; *address_str; ++address_str) {
    int8_t c = ascii_hex_to_int(*address_str);
    if (c == -1) return -1; // Unexpected character

    address = (address * 16) + c;
  }

  return address;
}

bool convert_bt_addr_hex_str_to_bd_addr(const char *hex_str, uint8_t *bd_addr, const unsigned int bd_addr_size) {
  const int len = strlen(hex_str);
  if (len != 12) {
    return false;
  }

  uint8_t* src = (uint8_t*) hex_str;
  uint8_t* dest = bd_addr + bd_addr_size - 1;

  for (unsigned int i = 0; i < bd_addr_size; ++i, src += 2, --dest) {
    *dest = ascii_hex_to_uint(src[0], src[1]);
  }

  return true;
}

uint32_t next_exponential_backoff(uint32_t *attempt, uint32_t initial_value, uint32_t max_value) {
  if (*attempt > 31) {
    return max_value;
  }
  uint32_t backoff_multiplier = 0x1 << (*attempt)++;
  uint32_t next_value = initial_value * backoff_multiplier;
  return MIN(next_value, max_value);
}

// Based on DJB2 Hash
uint32_t hash(const uint8_t *bytes, const uint32_t length) {
  uint32_t hash = 5381;

  if (length == 0) {
    return hash;
  }

  uint8_t c;
  const uint8_t *last_byte = bytes + length;
  while (bytes++ != last_byte) {
    c = *bytes;
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

const char *bool_to_str(bool b) {
  if (b) {
    return "yes";
  } else {
    return "no";
  }
}

// Override libgcc's table-driven __builtin_popcount implementation
#ifdef __arm__
int __popcountsi2(uint32_t val) {
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
