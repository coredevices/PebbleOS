/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/kernel/scratch.h>
#include <pbl/services/voice/voice_speex.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_ram.h>
#include <stubs_logging.h>

// The encoder state lives on the kernel heap
static int s_kernel_allocations;

void *pbl_zalloc_check(size_t bytes) {
  s_kernel_allocations++;
  return calloc(1, bytes);
}

void *pbl_realloc(void *ptr, size_t bytes) {
  if (!ptr) {
    s_kernel_allocations++;
  }
  return realloc(ptr, bytes);
}

void pbl_free(void *ptr) {
  if (ptr) {
    s_kernel_allocations--;
  }
  free(ptr);
}

static unsigned int prv_allocations(void) {
  return fake_ram_allocations();
}

static void prv_fill_frame(int16_t *frame, int n) {
  for (int i = 0; i < n; ++i) {
    frame[i] = (i % 40 < 20) ? 1000 : -1000;
  }
}

void test_voice_speex__cleanup(void) {
  cl_assert(!voice_speex_is_initialized());
  cl_assert_equal_i(prv_allocations(), 0);
  cl_assert_equal_i(s_kernel_allocations, 0);
  cl_assert_equal_i(fake_ram_allocations(), 0);
  fake_mutex_assert_all_unlocked();
}

void test_voice_speex__last_reference_frees_everything(void) {
  cl_assert(voice_speex_init());
  // The frame and bits buffers are in the volatile heap, the encoder state on the kernel heap
  const unsigned int allocations = prv_allocations();
  cl_assert_equal_i(allocations, 3);
  cl_assert(s_kernel_allocations > 0);
  cl_assert(fake_ram_allocations() > 0);

  cl_assert(voice_speex_init());
  cl_assert_equal_i(prv_allocations(), allocations);

  voice_speex_deinit();
  cl_assert(voice_speex_is_initialized());
  cl_assert_equal_i(prv_allocations(), allocations);
  cl_assert(fake_ram_allocations() > 0);

  voice_speex_deinit();
  cl_assert(!voice_speex_is_initialized());
}

void test_voice_speex__encodes_session_after_session(void) {
  for (int session = 0; session < 3; ++session) {
    cl_assert(voice_speex_init());
    // The microphone ring shares the heap with the encoder
    void *mic_ring = pbl_scratch_malloc(10240);
    cl_assert(mic_ring);

    const int frame_size = voice_speex_get_frame_size();
    cl_assert_equal_i(frame_size, 320);
    int16_t *frame = voice_speex_get_frame_buffer();
    prv_fill_frame(frame, frame_size);
    uint8_t encoded[200];
    cl_assert(voice_speex_encode_frame(frame, encoded, sizeof(encoded)) > 0);

    voice_speex_deinit();
    cl_assert(fake_ram_allocations() > 0);
    pbl_scratch_free(mic_ring);
    cl_assert_equal_i(fake_ram_allocations(), 0);
  }
}
