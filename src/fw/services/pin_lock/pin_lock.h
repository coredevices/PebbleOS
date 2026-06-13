/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PIN_LOCK_MIN_LEN 4u
#define PIN_LOCK_MAX_LEN 8u
#define PIN_LOCK_SALT_LEN 16u
#define PIN_LOCK_HASH_LEN 32u  // SHA256_DIGEST_SIZE

//! Compute hash = SHA256(salt || digits). `digits` is an array of values 0-9,
//! length `len` (4-8). Output buffer must be PIN_LOCK_HASH_LEN bytes.
void pin_lock_compute_hash(const uint8_t *salt, const uint8_t *digits, uint8_t len,
                           uint8_t out_hash[PIN_LOCK_HASH_LEN]);

//! Constant-time compare of two PIN_LOCK_HASH_LEN buffers. Returns true if equal.
bool pin_lock_hash_equal(const uint8_t *a, const uint8_t *b);
