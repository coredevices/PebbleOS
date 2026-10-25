/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include <string.h>

#include <bluetooth/lcpu_ring.h>

// The voice ring as the LCPU sets it up: 460 bytes.
#define RING_SIZE 460

static struct lcpu_audio_ring s_ring;
static uint8_t s_mem[RING_SIZE];

void test_lcpu_ring__initialize(void) {
  s_ring = (struct lcpu_audio_ring){.buffer_size = RING_SIZE};
  memset(s_mem, 0, sizeof(s_mem));
}

static void prv_fill(uint8_t *p, size_t len, uint8_t seed) {
  for (size_t i = 0; i < len; i++) {
    p[i] = (uint8_t)(seed + i);
  }
}

void test_lcpu_ring__empty_and_whole_packets_only(void) {
  uint8_t out[124];
  cl_assert_equal_i(lcpu_ring_data_len(&s_ring), 0);
  cl_assert_equal_i(lcpu_ring_get(&s_ring, s_mem, out, sizeof(out)), 0);

  uint8_t in[124];
  prv_fill(in, sizeof(in), 1);
  cl_assert_equal_i(lcpu_ring_put(&s_ring, s_mem, in, 100), 100);
  // A partial packet is left alone.
  cl_assert_equal_i(lcpu_ring_get(&s_ring, s_mem, out, sizeof(out)), 0);
  cl_assert_equal_i(lcpu_ring_data_len(&s_ring), 100);
}

// What the watch saw: the LCPU wrote three 124-byte packets (372) and the fourth did not fit.
void test_lcpu_ring__three_packets_fit_the_fourth_does_not(void) {
  uint8_t in[124];
  for (int i = 0; i < 3; i++) {
    prv_fill(in, sizeof(in), (uint8_t)i);
    cl_assert_equal_i(lcpu_ring_put(&s_ring, s_mem, in, sizeof(in)), sizeof(in));
  }
  cl_assert_equal_i(s_ring.write_index, 372);
  cl_assert_equal_i(s_ring.write_mirror, 0);
  cl_assert_equal_i(lcpu_ring_put(&s_ring, s_mem, in, sizeof(in)), 0);
  cl_assert_equal_i(lcpu_ring_space_len(&s_ring), 88);
}

// Packets go round the ring many times, across the end and on it, in order.
void test_lcpu_ring__wraps_with_mirror(void) {
  uint8_t in[124], out[124];
  for (int i = 0; i < 50; i++) {
    prv_fill(in, sizeof(in), (uint8_t)(i * 7));
    cl_assert_equal_i(lcpu_ring_put(&s_ring, s_mem, in, sizeof(in)), sizeof(in));
    cl_assert_equal_i(lcpu_ring_get(&s_ring, s_mem, out, sizeof(out)), sizeof(out));
    cl_assert(memcmp(in, out, sizeof(in)) == 0);
    cl_assert_equal_i(lcpu_ring_data_len(&s_ring), 0);
  }
}

// Writing up to the last byte flips the mirror with index 0, as the SDK does; full is told apart
// from empty by the mirror.
void test_lcpu_ring__exact_end_and_full(void) {
  uint8_t in[RING_SIZE], out[RING_SIZE];
  prv_fill(in, sizeof(in), 3);
  cl_assert_equal_i(lcpu_ring_put(&s_ring, s_mem, in, RING_SIZE), RING_SIZE);
  cl_assert_equal_i(s_ring.write_index, 0);
  cl_assert(s_ring.write_mirror != s_ring.read_mirror);
  cl_assert_equal_i(lcpu_ring_data_len(&s_ring), RING_SIZE);
  cl_assert_equal_i(lcpu_ring_space_len(&s_ring), 0);
  cl_assert_equal_i(lcpu_ring_get(&s_ring, s_mem, out, RING_SIZE), RING_SIZE);
  cl_assert(memcmp(in, out, sizeof(in)) == 0);
  cl_assert_equal_i(lcpu_ring_data_len(&s_ring), 0);
}

// Mirror and index of a side change together, in one 32-bit word.
void test_lcpu_ring__index_and_mirror_in_one_word(void) {
  uint8_t in[124], out[124];
  prv_fill(in, sizeof(in), 9);
  for (int i = 0; i < 4; i++) {
    lcpu_ring_put(&s_ring, s_mem, in, sizeof(in));
    lcpu_ring_get(&s_ring, s_mem, out, sizeof(out));
  }
  // 496 bytes went through: index 36 on the other side of the mirror.
  const uint32_t word = s_ring.write_word;
  cl_assert_equal_i(word >> 16, 36);
  cl_assert_equal_i(word & 0xFFFF, 0xFFFF);
  cl_assert_equal_i(s_ring.read_index, 36);
}

void test_lcpu_ring__peek_leaves_the_data(void) {
  uint8_t in[64], out[64];
  prv_fill(in, sizeof(in), 7);
  cl_assert_equal_i(lcpu_ring_put(&s_ring, s_mem, in, sizeof(in)), sizeof(in));
  cl_assert_equal_i(lcpu_ring_peek(&s_ring, s_mem, out, 4), 4);
  cl_assert_equal_i(lcpu_ring_data_len(&s_ring), sizeof(in));
  cl_assert_equal_i(lcpu_ring_peek(&s_ring, s_mem, out, sizeof(in) + 1), 0);
  cl_assert_equal_i(lcpu_ring_get(&s_ring, s_mem, out, sizeof(out)), sizeof(out));
  cl_assert(memcmp(in, out, sizeof(in)) == 0);
}

void test_lcpu_ring__air_bit_packets_wrap(void) {
  // With the air bits handed over, each packet is 4 + 60 bytes: 7 fit in the ring at a time.
  uint8_t in[64], out[64];
  for (int round = 0; round < 3; round++) {
    for (int i = 0; i < 7; i++) {
      prv_fill(in, sizeof(in), (uint8_t)(round * 7 + i));
      cl_assert_equal_i(lcpu_ring_put(&s_ring, s_mem, in, sizeof(in)), sizeof(in));
    }
    cl_assert_equal_i(lcpu_ring_put(&s_ring, s_mem, in, sizeof(in)), 0);
    for (int i = 0; i < 7; i++) {
      prv_fill(in, sizeof(in), (uint8_t)(round * 7 + i));
      cl_assert_equal_i(lcpu_ring_peek(&s_ring, s_mem, out, 4), 4);
      cl_assert_equal_i(lcpu_ring_get(&s_ring, s_mem, out, sizeof(out)), sizeof(out));
      cl_assert(memcmp(in, out, sizeof(in)) == 0);
    }
  }
}
