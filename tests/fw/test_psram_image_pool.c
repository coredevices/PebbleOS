/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>
#include <fake_mutex.h>
#include <fake_psram.h>
#include <kernel/psram_image_pool.h>
#include <stubs_logging.h>

void test_psram_image_pool__initialize(void) {
  fake_psram_reset();
}

void test_psram_image_pool__exhaustion_and_reuse(void) {
  void *slots[PSRAM_IMAGE_POOL_SLOTS];

  psram_retain_acquire();
  for (int i = 0; i < PSRAM_IMAGE_POOL_SLOTS; i++) {
    slots[i] = psram_image_pool_alloc();
    cl_assert(slots[i]);
    for (int j = 0; j < i; j++) {
      cl_assert(slots[i] != slots[j]);
    }
  }
  cl_assert_equal_i(psram_image_pool_used(), PSRAM_IMAGE_POOL_SLOTS);
  cl_assert(!psram_image_pool_alloc());

  psram_image_pool_free(slots[1]);
  cl_assert(psram_image_pool_alloc() == slots[1]);

  for (int i = 0; i < PSRAM_IMAGE_POOL_SLOTS; i++) {
    psram_image_pool_free(slots[i]);
  }
  cl_assert_equal_i(psram_image_pool_used(), 0);
  psram_retain_release();
  fake_mutex_assert_all_unlocked();
}
