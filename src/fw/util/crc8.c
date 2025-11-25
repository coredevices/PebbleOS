/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/crc8.h"

uint8_t crc8_calculate_bytes(const uint8_t *data, uint32_t data_len, bool big_endian) {
  uint8_t checksum = 0;
  crc8_calculate_bytes_streaming(data, data_len, &checksum, big_endian);
  return checksum;
}

void crc8_calculate_bytes_streaming(const uint8_t *data, uint32_t data_len, uint8_t *crc,
                                    bool big_endian) {
  // Optimal polynomial chosen based on
  // http://users.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
  // Note that this is different than the standard CRC-8 polynomial, because the
  // standard CRC-8 polynomial is not particularly good.

  // nibble lookup table for (x^8 + x^5 + x^3 + x^2 + x + 1)
  static const uint8_t lookup_table[] =
      { 0, 47, 94, 113, 188, 147, 226, 205, 87, 120, 9, 38, 235, 196,
        181, 154 };

  for (uint32_t i = 0; i < data_len * 2; i++) {
    uint8_t nibble;
    if (big_endian) {
      nibble = data[data_len - (i / 2) - 1];
    } else {
      nibble = data[i / 2];
    }
    if (i % 2 == 0) {
      nibble >>= 4;
    }
    int index = nibble ^ (*crc >> 4);
    *crc = lookup_table[index & 0xf] ^ ((*crc << 4) & 0xf0);
  }
}
