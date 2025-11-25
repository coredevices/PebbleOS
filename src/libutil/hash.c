/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/hash.h"

#include <stdint.h>

// Based on DJB2 Hash
uint32_t hash(const uint8_t *bytes, const uint32_t length) {
  uint32_t hash = 5381;

  if (length == 0) {
    return hash;
  }

  uint8_t c;
  const uint8_t *last_byte = bytes + length;
  while (bytes != last_byte) {
    c = *bytes;
    hash = ((hash << 5) + hash) + c;
    bytes++;
  }
  return hash;
}
