/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/cobs.h"

#include <stdint.h>
#include <stdbool.h>

#include "util/likely.h"

void cobs_streaming_decode_start(CobsDecodeContext * restrict ctx,
                                 void * restrict output_buffer,
                                 size_t length) {
  ctx->output = output_buffer;
  ctx->output_length = length;
  ctx->decoded_length = 0;
  ctx->payload_remaining = 0;
  ctx->block_is_terminated = false;
}

bool cobs_streaming_decode(CobsDecodeContext * restrict ctx, char in) {
  if (ctx->output == NULL) {
    // Uninitialized context or decoding has already failed.
    return false;
  }

  if (UNLIKELY(in == '\0')) {
    // Zero byte is never allowed in a COBS stream.
    ctx->output = NULL;
    return false;
  }

  if (UNLIKELY(ctx->payload_remaining == 0)) {
    // Incoming byte is a code byte.
    ctx->payload_remaining = (uint8_t)in - 1;
    if (ctx->decoded_length + ctx->payload_remaining +
        (ctx->block_is_terminated? 1 : 0) > ctx->output_length) {
      // Full decoded output cannot fit into the buffer; fail fast.
      ctx->output = NULL;
      return false;
    }
    // Since we've started a new block, write out the trailing zero left over
    // from the previous block. This wasn't done when the last character of the
    // previous block was written out as it could have been the last block in
    // the COBS stream.
    if (ctx->block_is_terminated) {
      ctx->output[ctx->decoded_length++] = '\0';
    }
    ctx->block_is_terminated = (in != '\xff');
  } else {
    // Incoming byte is contained within a COBS block.
    // It is safe to assume that there is enough space in the buffer for the
    // incoming byte as that check has already been performed when the code byte
    // was received.
    ctx->output[ctx->decoded_length++] = in;
    ctx->payload_remaining -= 1;
  }
  return true;
}

size_t cobs_streaming_decode_finish(CobsDecodeContext * restrict ctx) {
  if (ctx->output == NULL || ctx->payload_remaining != 0) {
    return SIZE_MAX;
  }
  return ctx->decoded_length;
}

size_t cobs_encode(void *dst_ptr, const void *src_ptr, size_t length) {
  const char *src = src_ptr;
  char *dst = dst_ptr;
  uint8_t code = 0x01;
  size_t code_idx = 0;
  size_t dst_idx = 1;

  for (size_t src_idx = 0; src_idx < length; ++src_idx) {
    if (src[src_idx] == '\0') {
      dst[code_idx] = code;
      code_idx = dst_idx++;
      code = 0x01;
    } else {
      dst[dst_idx++] = src[src_idx];
      code++;
      if (code == 0xff) {
        if (src_idx == length - 1) {
          // Special case: the final encoded block is 254 bytes long with no
          // zero after it. While it's technically a valid encoding if a
          // trailing zero is appended, it causes the output to be one byte
          // longer than it needs to be. This violates consistent overhead
          // contract and could overflow a carefully sized buffer.
          break;
        }
        dst[code_idx] = code;
        code_idx = dst_idx++;
        code = 0x01;
      }
    }
  }
  dst[code_idx] = code;
  return dst_idx;
}
