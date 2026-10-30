/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/mic.h>
#include <pbl/services/voice/voice_speex.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_psram.h>
#include <kernel/volatile_heap.h>
#include <stubs_logging.h>

extern uint8_t s_storage[CONFIG_KERNEL_VOLATILE_HEAP_SIZE];

// The encoder state lives on the kernel heap
static int s_kernel_allocations;

void *kernel_zalloc_check(size_t bytes) {
  s_kernel_allocations++;
  return calloc(1, bytes);
}

void *kernel_realloc(void *ptr, size_t bytes) {
  if (!ptr) {
    s_kernel_allocations++;
  }
  return realloc(ptr, bytes);
}

void kernel_free(void *ptr) {
  if (ptr) {
    s_kernel_allocations--;
  }
  free(ptr);
}

MicDevice *const MIC = NULL;

uint32_t mic_get_channels(MicDevice *this) {
  return 1;
}

static unsigned int prv_allocations(void) {
  unsigned int used, high_water, allocations;
  kernel_volatile_heap_stats(&used, &high_water, &allocations);
  return allocations;
}

static void prv_fill_frame(int16_t *frame, int n) {
  for (int i = 0; i < n; ++i) {
    frame[i] = (i % 40 < 20) ? 1000 : -1000;
  }
}

// Deep sleep with nothing holding the PSRAM: its contents are lost
static void prv_power_down(void) {
  fake_psram_power_down();
  memset(s_storage, 0xa5, sizeof(s_storage));
}

void test_voice_speex__initialize(void) {
  fake_psram_reset();
}

void test_voice_speex__cleanup(void) {
  cl_assert(!voice_speex_is_initialized());
  cl_assert_equal_i(prv_allocations(), 0);
  cl_assert_equal_i(s_kernel_allocations, 0);
  cl_assert_equal_i(fake_psram_holds(), 0);
  fake_mutex_assert_all_unlocked();
}

void test_voice_speex__last_reference_frees_everything(void) {
  cl_assert(voice_speex_init());
  // The frame and bits buffers are in the volatile heap, the encoder state on the kernel heap
  const unsigned int allocations = prv_allocations();
  cl_assert_equal_i(allocations, 3);
  cl_assert(s_kernel_allocations > 0);
  cl_assert_equal_i(fake_psram_holds(), 1);

  cl_assert(voice_speex_init());
  cl_assert_equal_i(prv_allocations(), allocations);

  voice_speex_deinit();
  cl_assert(voice_speex_is_initialized());
  cl_assert_equal_i(prv_allocations(), allocations);
  cl_assert_equal_i(fake_psram_holds(), 1);

  voice_speex_deinit();
  cl_assert(!voice_speex_is_initialized());
}

void test_voice_speex__encodes_after_psram_loss(void) {
  for (int session = 0; session < 3; ++session) {
    cl_assert(voice_speex_init());
    // The microphone ring shares the heap with the encoder
    void *mic_ring = kernel_volatile_malloc(10240);
    cl_assert(mic_ring);

    const int frame_size = voice_speex_get_frame_size();
    cl_assert_equal_i(frame_size, 320);
    int16_t *frame = voice_speex_get_frame_buffer();
    prv_fill_frame(frame, frame_size);
    uint8_t encoded[200];
    cl_assert(voice_speex_encode_frame(frame, encoded, sizeof(encoded)) > 0);

    voice_speex_deinit();
    cl_assert_equal_i(fake_psram_holds(), 1);
    kernel_volatile_free(mic_ring);
    cl_assert_equal_i(fake_psram_holds(), 0);
    prv_power_down();
  }
}
