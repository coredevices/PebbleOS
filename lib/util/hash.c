/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include <pbl/util/hash.h>

uint32_t hash(const uint8_t *bytes, const uint32_t length) {
  return hash_update(HASH_INIT, bytes, length);
}

// DJB2
uint32_t hash_update(uint32_t h, const void *data, size_t len) {
  const uint8_t *bytes = data;

  for (size_t i = 0; i < len; i++) {
    h = (h << 5) + h + bytes[i];
  }

  return h;
}
