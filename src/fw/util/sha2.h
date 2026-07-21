/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define SHA256_DIGEST_SIZE 32u
#define SHA256_BLOCK_SIZE  64u

typedef struct {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t buffer[SHA256_BLOCK_SIZE];
  size_t buffer_len;
} SHA256Context;

void sha256_init(SHA256Context *ctx);
void sha256_update(SHA256Context *ctx, const uint8_t *data, size_t len);
void sha256_final(SHA256Context *ctx, uint8_t out[SHA256_DIGEST_SIZE]);

//! One-shot convenience wrapper.
void sha256_hash(const uint8_t *data, size_t len, uint8_t out[SHA256_DIGEST_SIZE]);
