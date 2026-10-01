/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/buffer.h"

void pbl_buffer_init(struct pbl_buffer *buffer, size_t capacity) {
}

size_t pbl_buffer_add(struct pbl_buffer *buffer, const void *data, size_t length) {
  return 0;
}

size_t pbl_buffer_remove(struct pbl_buffer *buffer, size_t offset, size_t length) {
  return 0;
}

void pbl_buffer_clear(struct pbl_buffer *buffer) {
}
