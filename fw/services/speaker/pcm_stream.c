/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pcm_stream.h"

#include "kernel/pbl_malloc.h"

#include <string.h>

bool pcm_stream_init(PcmStreamState *s, uint32_t size_bytes) {
  memset(s, 0, sizeof(*s));
  s->buffer = kernel_malloc(size_bytes);
  if (!s->buffer) {
    return false;
  }
  s->size = size_bytes;
  return true;
}

uint32_t pcm_stream_write(PcmStreamState *s, const void *data, uint32_t n) {
  if (!s->buffer || s->closing) {
    return 0;
  }

  uint32_t space = s->size - s->count;
  if (n > space) {
    n = space;
  }

  const uint8_t *src = data;
  for (uint32_t i = 0; i < n; i++) {
    s->buffer[s->write_pos] = src[i];
    s->write_pos = (s->write_pos + 1) % s->size;
  }
  s->count += n;

  return n;
}

uint32_t pcm_stream_read(PcmStreamState *s, void *out, uint32_t max) {
  if (!s->buffer) {
    return 0;
  }

  uint32_t avail = s->count;
  if (max > avail) {
    max = avail;
  }

  uint8_t *dst = out;
  for (uint32_t i = 0; i < max; i++) {
    dst[i] = s->buffer[s->read_pos];
    s->read_pos = (s->read_pos + 1) % s->size;
  }
  s->count -= max;

  return max;
}

void pcm_stream_mark_closing(PcmStreamState *s) {
  s->closing = true;
}

bool pcm_stream_is_done(PcmStreamState *s) {
  return s->closing && s->count == 0;
}

void pcm_stream_deinit(PcmStreamState *s) {
  if (s->buffer) {
    kernel_free(s->buffer);
  }
  memset(s, 0, sizeof(*s));
}

uint32_t pcm_stream_available(const PcmStreamState *s) {
  return s->count;
}

uint32_t pcm_stream_free_space(const PcmStreamState *s) {
  return s->size - s->count;
}
