/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/util/assert.h>
#include <pbl/util/buffer.h>

void pbl_buffer_init(struct pbl_buffer *buffer, size_t capacity) {
  buffer->length = capacity;
  buffer->bytes_written = 0;
}

size_t pbl_buffer_add(struct pbl_buffer *buffer, const void *data, size_t length) {
  UTIL_ASSERT(buffer && data && length);

  if (buffer->length - buffer->bytes_written < length) {
    return 0;
  }

  memcpy(&buffer->data[buffer->bytes_written], data, length);
  buffer->bytes_written += length;
  return length;
}

size_t pbl_buffer_remove(struct pbl_buffer *buffer, size_t offset, size_t length) {
  UTIL_ASSERT(offset + length <= buffer->bytes_written);

  memmove(&buffer->data[offset], &buffer->data[offset + length],
          buffer->bytes_written - length - offset);
  buffer->bytes_written -= length;
  return length;
}

void pbl_buffer_clear(struct pbl_buffer *buffer) {
  buffer->bytes_written = 0;
}
