/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/mic.h>
#include <pbl/services/voice/voice_speex.h>

#include <clar.h>
#include <fake_mutex.h>
#include <stubs_logging.h>

static int s_live_allocations;

void *pbl_malloc_check(size_t bytes) {
  s_live_allocations++;
  return malloc(bytes);
}

void *pbl_zalloc_check(size_t bytes) {
  s_live_allocations++;
  return calloc(1, bytes);
}

void *pbl_realloc(void *ptr, size_t bytes) {
  if (!ptr) {
    s_live_allocations++;
  }
  return realloc(ptr, bytes);
}

void pbl_free(void *ptr) {
  if (ptr) {
    s_live_allocations--;
  }
  free(ptr);
}

MicDevice *const MIC = NULL;

uint32_t mic_get_channels(MicDevice *this) {
  return 1;
}

static void prv_fill_frame(int16_t *frame, int n) {
  for (int i = 0; i < n; ++i) {
    frame[i] = (i % 40 < 20) ? 1000 : -1000;
  }
}

void test_voice_speex__cleanup(void) {
  cl_assert(!voice_speex_is_initialized());
  cl_assert_equal_i(s_live_allocations, 0);
  fake_mutex_assert_all_unlocked();
}

void test_voice_speex__last_reference_frees_everything(void) {
  cl_assert(voice_speex_init());
  const int allocations = s_live_allocations;
  cl_assert(allocations > 0);

  cl_assert(voice_speex_init());
  cl_assert_equal_i(s_live_allocations, allocations);

  voice_speex_deinit();
  cl_assert(voice_speex_is_initialized());
  cl_assert_equal_i(s_live_allocations, allocations);

  voice_speex_deinit();
  cl_assert(!voice_speex_is_initialized());
  cl_assert_equal_i(s_live_allocations, 0);
}

void test_voice_speex__encodes_after_reinit(void) {
  for (int session = 0; session < 3; ++session) {
    cl_assert(voice_speex_init());
    const int frame_size = voice_speex_get_frame_size();
    cl_assert_equal_i(frame_size, 320);
    int16_t *frame = voice_speex_get_frame_buffer();
    prv_fill_frame(frame, frame_size);
    uint8_t encoded[200];
    cl_assert(voice_speex_encode_frame(frame, encoded, sizeof(encoded)) > 0);
    voice_speex_deinit();
  }
}
