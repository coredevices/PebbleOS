/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sle.h"

#include "system/passert.h"

// See waftools/sparse_length_encoding.py for more info on SLE encoding/decoding

void sle_decode_init(SLEDecodeContext *ctx, const void *sle_buffer) {
  PBL_ASSERTN(ctx && sle_buffer);
  const uint8_t *buffer = (const uint8_t *)sle_buffer;
  ctx->escape = *(buffer++);
  ctx->sle_buffer = buffer;
  ctx->zeros_remaining = 0;
}

bool sle_decode(SLEDecodeContext *ctx, uint8_t *out) {
  if (!ctx->sle_buffer) {
    return false;
  }
  if (ctx->zeros_remaining) {
    *out = 0;
    --ctx->zeros_remaining;
    return true;
  }
  uint8_t byte = *(ctx->sle_buffer++);
  if (byte != ctx->escape) {
    *out = byte;
  } else {
    byte = *(ctx->sle_buffer++);
    if (byte == 0x00) {
      // end of stream
      ctx->sle_buffer = NULL;
      return false;
    } else if (byte == 0x01) {
      // literal escape byte
      *out = ctx->escape;
    } else {
      // a sequence of zeros
      if ((byte & 0x80) == 0) {
        // the count is 1 byte (1-127)
        ctx->zeros_remaining = byte - 1;
      } else {
        ctx->zeros_remaining = (((byte & 0x7f) << 8) | *(ctx->sle_buffer++)) + 0x80 - 1;
      }
      *out = 0;
    }
  }
  return true;
}
