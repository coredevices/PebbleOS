/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

//! Linear byte buffer with its storage appended to the header.
//! Allocate sizeof(struct pbl_buffer) + capacity bytes and call pbl_buffer_init().
struct pbl_buffer {
  size_t length;
  size_t bytes_written;
  uint8_t data[];
};

//! @param buffer Buffer to initialize
//! @param capacity Number of bytes available after the header
void pbl_buffer_init(struct pbl_buffer *buffer, size_t capacity);

//! Appends data to the buffer.
//! @return length, or 0 if the data does not fit
size_t pbl_buffer_add(struct pbl_buffer *buffer, const void *data, size_t length);

//! Removes bytes at offset, closing the gap.
//! @return length
size_t pbl_buffer_remove(struct pbl_buffer *buffer, size_t offset, size_t length);

void pbl_buffer_clear(struct pbl_buffer *buffer);
