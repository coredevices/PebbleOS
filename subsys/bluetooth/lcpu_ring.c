/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "lcpu_ring.h"

// The mirror goes into the low half of the word, the index into the high half.
static uint32_t prv_word(uint16_t mirror, uint16_t index) {
  return ((uint32_t)index << 16) | mirror;
}

size_t lcpu_ring_data_len(const volatile struct lcpu_audio_ring *r) {
  const uint32_t rd = r->read_word;
  const uint32_t wr = r->write_word;
  const uint16_t read_index = rd >> 16, write_index = wr >> 16;
  const size_t size = (size_t)r->buffer_size;
  if (read_index == write_index) {
    return ((rd & 0xFF) == (wr & 0xFF)) ? 0 : size;
  }
  return write_index > read_index ? (size_t)(write_index - read_index)
                                  : size - (size_t)(read_index - write_index);
}

size_t lcpu_ring_space_len(const volatile struct lcpu_audio_ring *r) {
  return (size_t)r->buffer_size - lcpu_ring_data_len(r);
}

size_t lcpu_ring_peek(const volatile struct lcpu_audio_ring *r, const volatile uint8_t *mem,
                      uint8_t *out, size_t len) {
  if (len == 0 || lcpu_ring_data_len(r) < len) {
    return 0;
  }
  const size_t size = (size_t)r->buffer_size;
  const size_t index = r->read_index;
  const size_t first = (size - index > len) ? len : size - index;
  for (size_t i = 0; i < first; i++) {
    out[i] = mem[index + i];
  }
  for (size_t i = first; i < len; i++) {
    out[i] = mem[i - first];
  }
  return len;
}

size_t lcpu_ring_get(volatile struct lcpu_audio_ring *r, const volatile uint8_t *mem, uint8_t *out,
                     size_t len) {
  if (lcpu_ring_peek(r, mem, out, len) == 0) {
    return 0;
  }
  const size_t size = (size_t)r->buffer_size;
  const size_t index = r->read_index;
  const size_t first = (size - index > len) ? len : size - index;
  if (first < len || index + len == size) {
    // Into the other side of the mirror.
    r->read_word = prv_word((uint16_t)~r->read_mirror, (uint16_t)(len - first));
  } else {
    r->read_word = prv_word(r->read_mirror, (uint16_t)(index + len));
  }
  return len;
}

size_t lcpu_ring_put(volatile struct lcpu_audio_ring *r, volatile uint8_t *mem, const uint8_t *in,
                     size_t len) {
  if (len == 0 || lcpu_ring_space_len(r) < len) {
    return 0;
  }
  const size_t size = (size_t)r->buffer_size;
  const size_t index = r->write_index;
  const size_t first = (size - index > len) ? len : size - index;
  for (size_t i = 0; i < first; i++) {
    mem[index + i] = in[i];
  }
  for (size_t i = first; i < len; i++) {
    mem[i - first] = in[i];
  }
  if (first < len || index + len == size) {
    r->write_word = prv_word((uint16_t)~r->write_mirror, (uint16_t)(len - first));
  } else {
    r->write_word = prv_word(r->write_mirror, (uint16_t)(index + len));
  }
  return len;
}
