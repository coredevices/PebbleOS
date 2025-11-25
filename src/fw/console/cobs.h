/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! An implementation of Consistent Overhead Byte Stuffing
//!
//! http://conferences.sigcomm.org/sigcomm/1997/papers/p062.pdf
//! http://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing

//! Evaluates to the offset required when encoding in-place
#define COBS_OVERHEAD(n) (((n) + 253) / 254)
//! Evaluates to the maximum buffer size required to hold n bytes of data
//! after COBS encoding.
#define MAX_SIZE_AFTER_COBS_ENCODING(n) ((n) + COBS_OVERHEAD(n))

typedef struct CobsDecodeContext {
  char *output;
  size_t output_length;
  size_t decoded_length;
  uint8_t payload_remaining;
  bool block_is_terminated;
} CobsDecodeContext;

//! Initialize the COBS decoding context.
void cobs_streaming_decode_start(CobsDecodeContext * restrict ctx,
                                 void * restrict output_buffer,
                                 size_t length);

//! Decode a byte in the COBS stream.
//!
//! @return false if decoding has failed.
bool cobs_streaming_decode(CobsDecodeContext * restrict ctx, char in);

//! Complete a COBS stream.
//!
//! @return length of decoded stream, or SIZE_MAX if decoding has failed.
size_t cobs_streaming_decode_finish(CobsDecodeContext * restrict ctx);

//! COBS-encode a buffer out to another buffer.
//!
//! @param [out] dst destination buffer. The buffer must be at least
//!                 MAX_SIZE_AFTER_COBS_ENCODING(length) bytes long.
//! @param [in] src source buffer
//! @param      length length of src
size_t cobs_encode(void * restrict dst, const void * restrict src,
                   size_t length);
