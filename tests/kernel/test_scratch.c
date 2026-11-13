/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/kernel/scratch.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_ram.h>

#define KIB 1024U

void test_scratch__initialize(void) {
  fake_ram_set_get_result(0);
}

void test_scratch__cleanup(void) {
  cl_assert_equal_i(fake_ram_allocations(), 0);
  cl_assert(!fake_ram_in_use());
  fake_mutex_assert_all_unlocked();
}

void test_scratch__memory_in_use_while_allocated(void) {
  uint8_t *a = pbl_scratch_malloc(100);
  uint8_t *b = pbl_scratch_malloc(3000);

  cl_assert(a && b);
  cl_assert(fake_ram_in_use());
  memset(a, 1, 100);
  memset(b, 2, 3000);

  pbl_scratch_free(a);
  cl_assert(fake_ram_in_use());
  cl_assert_equal_i(b[2999], 2);
  pbl_scratch_free(b);
  cl_assert(!fake_ram_in_use());
}

void test_scratch__first_fit_and_reuse(void) {
  uint8_t *a = pbl_scratch_malloc(KIB + 1);
  uint8_t *b = pbl_scratch_malloc(10);
  uint8_t *c = pbl_scratch_malloc(KIB);
  struct pbl_scratch_stats stats;

  cl_assert(a && b && c);
  cl_assert_equal_i(b - a, 2 * KIB);
  cl_assert_equal_i(c - b, KIB);
  pbl_scratch_get_stats(&stats);
  cl_assert_equal_i(stats.used, 4 * KIB);
  cl_assert_equal_i(stats.allocations, 3);
  cl_assert_equal_i(stats.max_free, CONFIG_SCRATCH_HEAP_SIZE - 4 * KIB);

  // The hole takes what fits, the rest goes after
  pbl_scratch_free(b);
  cl_assert(pbl_scratch_malloc(2 * KIB) == c + KIB);
  cl_assert(pbl_scratch_malloc(KIB) == b);

  pbl_scratch_free(a);
  pbl_scratch_free(b);
  pbl_scratch_free(c);
  pbl_scratch_free(c + KIB);

  pbl_scratch_get_stats(&stats);
  cl_assert_equal_i(stats.used, 0);
  cl_assert_equal_i(stats.peak, 6 * KIB);
  cl_assert_equal_i(stats.max_free, CONFIG_SCRATCH_HEAP_SIZE);
}

void test_scratch__adjacent_blocks_are_freed_apart(void) {
  uint8_t *a = pbl_scratch_malloc(3 * KIB);
  uint8_t *b = pbl_scratch_malloc(2 * KIB);

  pbl_scratch_free(a);
  cl_assert_equal_i(fake_ram_allocations(), 1);
  // b is untouched: the same room is there again, and no more
  cl_assert(pbl_scratch_malloc(3 * KIB) == a);
  pbl_scratch_free(a);
  pbl_scratch_free(b);
}

void test_scratch__no_room(void) {
  cl_assert(!pbl_scratch_malloc(CONFIG_SCRATCH_HEAP_SIZE + 1));
  cl_assert(!pbl_scratch_malloc(SIZE_MAX));
  cl_assert(!pbl_scratch_malloc(0));
  cl_assert(!fake_ram_in_use());

  uint8_t *a = pbl_scratch_malloc(CONFIG_SCRATCH_HEAP_SIZE);
  cl_assert(a);
  cl_assert(!pbl_scratch_malloc(1));
  pbl_scratch_free(a);
  pbl_scratch_free(NULL);
}

void test_scratch__memory_that_does_not_power_on(void) {
  fake_ram_set_get_result(-EIO);
  cl_assert(!pbl_scratch_malloc(10));
  cl_assert_equal_i(fake_ram_allocations(), 0);

  fake_ram_set_get_result(0);
  void *a = pbl_scratch_malloc(10);
  cl_assert(a);
  pbl_scratch_free(a);
}

void test_scratch__zalloc(void) {
  uint8_t *a = pbl_scratch_zalloc(2000);

  cl_assert(a);
  for (size_t i = 0; i < 2000; i++) {
    cl_assert_equal_i(a[i], 0);
  }
  pbl_scratch_free(a);
}
