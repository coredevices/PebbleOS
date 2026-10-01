/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/crc/crc.h>

// Nybble-wide lookup table for polynomial 0x2F
static const uint8_t s_lut[16] = {0,  47,  94, 113, 188, 147, 226, 205,
                                  87, 120, 9,  38,  235, 196, 181, 154};

static uint8_t prv_byte(uint8_t crc, uint8_t byte) {
  crc = s_lut[((crc >> 4) ^ (byte >> 4)) & 0x0f] ^ (uint8_t)(crc << 4);
  crc = s_lut[((crc >> 4) ^ byte) & 0x0f] ^ (uint8_t)(crc << 4);
  return crc;
}

uint8_t pbl_crc8(uint8_t crc, const void *data, size_t len) {
  const uint8_t *bytes = data;
  for (size_t i = 0; i < len; i++) {
    crc = prv_byte(crc, bytes[i]);
  }
  return crc;
}

uint8_t pbl_crc8_reversed(uint8_t crc, const void *data, size_t len) {
  const uint8_t *bytes = data;
  while (len--) {
    crc = prv_byte(crc, bytes[len]);
  }
  return crc;
}
