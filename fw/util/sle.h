/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  const uint8_t *sle_buffer;
  uint16_t zeros_remaining;
  uint8_t escape;
} SLEDecodeContext;

//! Initialize the decode context to decode the given buffer.
void sle_decode_init(SLEDecodeContext *ctx, const void *sle_buffer);

//! Decode the next byte in the incoming buffer.
bool sle_decode(SLEDecodeContext *ctx, uint8_t *out);
