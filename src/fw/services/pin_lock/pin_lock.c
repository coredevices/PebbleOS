/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#include "services/pin_lock/pin_lock.h"

#include "util/sha2.h"

void pin_lock_compute_hash(const uint8_t *salt, const uint8_t *digits, uint8_t len,
                           uint8_t out_hash[PIN_LOCK_HASH_LEN]) {
  SHA256Context ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, salt, PIN_LOCK_SALT_LEN);
  sha256_update(&ctx, digits, len);
  sha256_final(&ctx, out_hash);
}

bool pin_lock_hash_equal(const uint8_t *a, const uint8_t *b) {
  uint8_t diff = 0;
  for (unsigned i = 0; i < PIN_LOCK_HASH_LEN; i++) {
    diff |= (uint8_t)(a[i] ^ b[i]);
  }
  return diff == 0;
}
