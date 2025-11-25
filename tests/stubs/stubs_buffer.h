/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

size_t buffer_get_bytes_remaining(Buffer *b) {
  return 0;
}

size_t buffer_add(Buffer* const b, const uint8_t* const data, const size_t length) {
  return 0;
}

size_t buffer_remove(Buffer* const b, const size_t offset, const size_t length) {
  return 0;
}

Buffer* buffer_create(const size_t size_bytes) {
  return NULL;
}

void buffer_init(Buffer * const buffer, const size_t length) {
}

void buffer_clear(Buffer * const buffer) {
}

bool buffer_is_empty(Buffer * const buffer) {
  return true;
}
