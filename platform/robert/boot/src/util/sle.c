/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sle.h"

#include "system/passert.h"

// See waftools/sparse_length_encoding.py for more info on SLE encoding/decoding

typedef struct {
  const uint8_t *data;
  uint32_t index;
  uint32_t length;
} ReadByteStream;

typedef struct {
  uint8_t *data;
  uint32_t index;
  uint32_t length;
} WriteByteStream;

static uint8_t prv_byte_stream_read(ReadByteStream *stream) {
  PBL_ASSERTN(stream->index < stream->length);
  return stream->data[stream->index++];
}

static void prv_byte_stream_write(WriteByteStream *stream, uint8_t data) {
  PBL_ASSERTN(stream->index < stream->length);
  stream->data[stream->index++] = data;
}

uint32_t sle_decode(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t out_len) {
  ReadByteStream in_stream = {
    .data = in,
    .length = in_len
  };
  WriteByteStream out_stream = {
    .data = out,
    .length = out_len
  };

  const uint8_t escape = prv_byte_stream_read(&in_stream);
  while (true) {
    const uint8_t byte = prv_byte_stream_read(&in_stream);
    if (byte != escape) {
      // simply write the byte into the output stream
      prv_byte_stream_write(&out_stream, byte);
      continue;
    }

    // read the escape code
    const uint8_t code = prv_byte_stream_read(&in_stream);
    if (code == 0) {
      // end of stream
      break;
    } else if (code == 1) {
      // literal escape byte
      prv_byte_stream_write(&out_stream, escape);
    } else {
      // a sequence of zeros
      uint16_t count;
      if ((code & 0x80) == 0) {
        // the count is only 1 byte (1-127)
        count = code;
      } else {
        // the count is 2 bytes
        count = (((uint16_t)(code & 0x7f) << 8) | prv_byte_stream_read(&in_stream)) + 0x80;
      }
      for (int i = 0; i < count; ++i) {
        prv_byte_stream_write(&out_stream, 0);
      }
    }
  }
  return out_stream.index;
}
