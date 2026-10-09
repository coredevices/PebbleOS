/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_circular_buffer Circular buffer
 * @ingroup util
 * @brief Single-reader, single-writer byte ring buffer.
 *
 * Data is written and consumed in a circular fashion over caller-provided storage: a write that
 * reaches the end of the storage wraps around to its start, using space freed by consumed data.
 * Reads do not consume, so the reader can work on the data in place and consume it afterwards.
 * There is no locking.
 *
 * @code{.c}
 * static uint8_t s_storage[256];
 * static CircularBuffer s_buf;
 *
 * circular_buffer_init(&s_buf, s_storage, sizeof(s_storage));
 * circular_buffer_write(&s_buf, msg, msg_len);
 *
 * const uint8_t *data;
 * uint16_t len;
 * uint16_t avail = circular_buffer_get_read_space_remaining(&s_buf);
 * while (avail > 0) {
 *   circular_buffer_read(&s_buf, avail, &data, &len); // len stops at the end of the storage
 *   process(data, len);
 *   circular_buffer_consume(&s_buf, len);
 *   avail -= len;
 * }
 * @endcode
 * @{
 */

/** @brief Circular buffer state. */
typedef struct CircularBuffer {
  /** Storage. */
  uint8_t *buffer;
  /** A circular_buffer_write_prepare() is pending its circular_buffer_write_finish(). */
  bool write_in_progress;
  /** Rewind to the start of the storage when the buffer empties. */
  bool auto_reset;
  /** Size of @ref buffer in bytes. */
  uint16_t buffer_size;
  /** Offset in @ref buffer to read from next. */
  uint16_t read_index;
  /** Bytes of valid data starting at @ref read_index. */
  uint16_t data_length;
} CircularBuffer;

/**
 * @brief Initialize a circular buffer, with auto reset enabled.
 *
 * @param[out] buffer Circular buffer.
 * @param storage Storage, must outlive @p buffer.
 * @param storage_size Size of @p storage in bytes.
 */
void circular_buffer_init(CircularBuffer *buffer, uint8_t *storage, uint16_t storage_size);

/**
 * @brief Initialize a circular buffer, choosing whether it auto resets.
 *
 * With auto reset, the read and write positions go back to the start of the storage whenever
 * circular_buffer_consume() empties the buffer, to reduce wrapping. Without it the buffer always
 * wraps and old data stays in the storage, which is handy for post-mortem analysis of debug logs.
 *
 * @param[out] buffer Circular buffer.
 * @param storage Storage, must outlive @p buffer.
 * @param storage_size Size of @p storage in bytes.
 * @param auto_reset Whether to auto reset.
 */
void circular_buffer_init_ex(CircularBuffer *buffer, uint8_t *storage, uint16_t storage_size,
                             bool auto_reset);

/**
 * @brief Copy data into the circular buffer.
 *
 * @param buffer Circular buffer.
 * @param data Data to write.
 * @param length Number of bytes to write.
 * @return true on success, false if there is not enough space (nothing is written).
 */
bool circular_buffer_write(CircularBuffer *buffer, const void *data, uint16_t length);

/**
 * @brief Get a contiguous area of the circular buffer to write to directly.
 *
 * Call circular_buffer_write_finish() once done writing, so the buffer accounts for the data.
 * Only one such write may be in progress.
 *
 * @param buffer Circular buffer.
 * @param[out] data_out Start of the writable area, or NULL if there is no space or a write is
 * already in progress.
 * @return Number of bytes that can be written at @p data_out, 0 if none.
 */
uint16_t circular_buffer_write_prepare(CircularBuffer *buffer, uint8_t **data_out);

/**
 * @brief Commit data written after circular_buffer_write_prepare().
 *
 * @param buffer Circular buffer.
 * @param written_length Number of bytes written at the area returned by
 * circular_buffer_write_prepare().
 */
void circular_buffer_write_finish(CircularBuffer *buffer, uint16_t written_length);

/**
 * @brief Get a pointer to the oldest data, without consuming it.
 *
 * When the requested data wraps around the end of the storage, only the part up to the end is
 * returned and @p length_out is smaller than @p length; read again after consuming it to get the
 * rest. @p data_out stays valid until the data is consumed.
 *
 * @param buffer Circular buffer.
 * @param length Number of bytes to read.
 * @param[out] data_out Start of the data.
 * @param[out] length_out Number of contiguous bytes at @p data_out.
 * @return false if there are fewer than @p length bytes in the buffer.
 */
bool circular_buffer_read(const CircularBuffer *buffer, uint16_t length, const uint8_t **data_out,
                          uint16_t *length_out);

/**
 * @brief Copy the oldest data out of the circular buffer, without consuming it.
 *
 * Same as circular_buffer_copy_offset() with an offset of 0.
 *
 * @param buffer Circular buffer.
 * @param[out] data_out Destination.
 * @param length Maximum number of bytes to copy.
 * @return Number of bytes copied.
 */
uint16_t circular_buffer_copy(const CircularBuffer *buffer, void *data_out, uint16_t length);

/**
 * @brief Copy data out of the circular buffer, handling the wrap, without consuming it.
 *
 * @param buffer Circular buffer.
 * @param start_offset Number of bytes of the oldest data to skip.
 * @param[out] data_out Destination.
 * @param length Maximum number of bytes to copy.
 * @return Number of bytes copied, 0 if there is no data past @p start_offset.
 */
uint16_t circular_buffer_copy_offset(const CircularBuffer *buffer, uint16_t start_offset,
                                     uint8_t *data_out, uint16_t length);

/**
 * @brief Get the oldest data as one contiguous array, copying it only when it wraps.
 *
 * @param buffer Circular buffer.
 * @param[out] data_out Start of the data, NULL if the copy could not be allocated.
 * @param length Number of bytes to get.
 * @param malloc_imp Allocator for the copy.
 * @param[out] caller_should_free Set when @p data_out is a copy the caller must free.
 * @return false if there are fewer than @p length bytes in the buffer or the allocation failed.
 */
bool circular_buffer_read_or_copy(const CircularBuffer *buffer, uint8_t **data_out, size_t length,
                                  void *(*malloc_imp)(size_t), bool *caller_should_free);

/**
 * @brief Remove the oldest data from the circular buffer.
 *
 * @param buffer Circular buffer.
 * @param length Number of bytes to remove.
 * @return false if there are fewer than @p length bytes in the buffer (nothing is removed).
 */
bool circular_buffer_consume(CircularBuffer *buffer, uint16_t length);

/**
 * @brief Get the free space.
 *
 * @param buffer Circular buffer.
 * @return Number of bytes circular_buffer_write() can take.
 */
uint16_t circular_buffer_get_write_space_remaining(const CircularBuffer *buffer);

/**
 * @brief Get the amount of data.
 *
 * @param buffer Circular buffer.
 * @return Number of bytes available to read.
 */
uint16_t circular_buffer_get_read_space_remaining(const CircularBuffer *buffer);

/** @} */
