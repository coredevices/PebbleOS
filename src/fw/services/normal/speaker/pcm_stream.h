/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PCM_STREAM_DEFAULT_SIZE_BYTES 8192  // 8KB

typedef struct {
  uint8_t *buffer;
  uint32_t size;       // total capacity in bytes
  uint32_t read_pos;
  uint32_t write_pos;
  uint32_t count;      // number of bytes currently buffered
  bool closing;        // no more writes expected, drain remaining
} PcmStreamState;

//! Initialize a PCM stream ring buffer.
//! @param s State to initialize
//! @param size_bytes Ring buffer capacity in bytes
//! @return true on success, false if allocation failed
bool pcm_stream_init(PcmStreamState *s, uint32_t size_bytes);

//! Write data into the ring buffer.
//! @param s Stream state
//! @param data Source bytes
//! @param n Number of bytes to write
//! @return Number of bytes actually written (may be less if buffer is full)
uint32_t pcm_stream_write(PcmStreamState *s, const void *data, uint32_t n);

//! Read data from the ring buffer.
//! @param s Stream state
//! @param out Destination buffer
//! @param max Maximum number of bytes to read
//! @return Number of bytes actually read
uint32_t pcm_stream_read(PcmStreamState *s, void *out, uint32_t max);

//! Mark the stream as closing (no more writes). Playback continues until drained.
void pcm_stream_mark_closing(PcmStreamState *s);

//! Check if the stream is done (closing and fully drained).
bool pcm_stream_is_done(PcmStreamState *s);

//! Free resources associated with the stream.
void pcm_stream_deinit(PcmStreamState *s);

//! Get the number of bytes available for reading.
uint32_t pcm_stream_available(const PcmStreamState *s);

//! Get the number of bytes of free space for writing.
uint32_t pcm_stream_free_space(const PcmStreamState *s);
