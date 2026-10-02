/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_buffer Linear buffer
 * @ingroup util
 * @brief Append-only byte buffer with its storage after the header.
 *
 * @code{.c}
 * struct pbl_buffer *buf = malloc(sizeof(struct pbl_buffer) + 64);
 *
 * pbl_buffer_init(buf, 64);
 * pbl_buffer_add(buf, &hdr, sizeof(hdr));
 * @endcode
 * @{
 */

/** @brief Linear byte buffer. Allocate @c sizeof(struct pbl_buffer) plus the capacity. */
struct pbl_buffer {
  /** Capacity of @ref data in bytes. */
  size_t length;
  /** Bytes used in @ref data. */
  size_t bytes_written;
  /** Storage. */
  uint8_t data[];
};

/**
 * @brief Initialize an empty buffer.
 *
 * @param[out] buffer Buffer.
 * @param capacity Number of bytes available after the header.
 */
void pbl_buffer_init(struct pbl_buffer *buffer, size_t capacity);

/**
 * @brief Append data to the buffer.
 *
 * @param buffer Buffer.
 * @param data Data, not NULL.
 * @param length Number of bytes, not 0.
 * @return @p length, or 0 if the data does not fit (nothing is added).
 */
size_t pbl_buffer_add(struct pbl_buffer *buffer, const void *data, size_t length);

/**
 * @brief Remove bytes from the buffer, closing the gap.
 *
 * @param buffer Buffer.
 * @param offset Offset of the first byte to remove.
 * @param length Number of bytes to remove; the range must be within the used bytes.
 * @return @p length.
 */
size_t pbl_buffer_remove(struct pbl_buffer *buffer, size_t offset, size_t length);

/**
 * @brief Empty the buffer.
 *
 * @param buffer Buffer.
 */
void pbl_buffer_clear(struct pbl_buffer *buffer);

/** @} */
