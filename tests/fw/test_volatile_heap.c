/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/psram.h>

#include <clar.h>
#include <fake_mutex.h>
#include <kernel/volatile_heap.h>
#include <stubs_logging.h>

extern uint8_t s_storage[CONFIG_KERNEL_VOLATILE_HEAP_SIZE];
static unsigned int s_holds;
static uint32_t s_epoch;

void psram_retain_acquire(void) {
  s_holds++;
}

void psram_retain_release(void) {
  cl_assert(s_holds > 0);
  s_holds--;
}

bool psram_is_retained(void) {
  return s_holds > 0;
}

uint32_t psram_epoch(void) {
  return s_epoch;
}

// Deep power-down with no hold: the contents are lost
static void prv_power_down(void) {
  cl_assert_equal_i(s_holds, 0);
  memset(s_storage, 0xa5, sizeof(s_storage));
  s_epoch++;
}

void test_volatile_heap__cleanup(void) {
  cl_assert_equal_i(s_holds, 0);
  fake_mutex_assert_all_unlocked();
}

void test_volatile_heap__holds_until_last_free(void) {
  void *a = kernel_volatile_malloc(256);
  void *b = kernel_volatile_malloc(512);
  cl_assert(a && b);
  cl_assert_equal_i(s_holds, 1);
  kernel_volatile_free(a);
  cl_assert_equal_i(s_holds, 1);
  kernel_volatile_free(b);
  cl_assert_equal_i(s_holds, 0);
}

void test_volatile_heap__failed_first_allocation_releases_hold(void) {
  cl_assert(!kernel_volatile_malloc(CONFIG_KERNEL_VOLATILE_HEAP_SIZE * 2));
  cl_assert_equal_i(s_holds, 0);
  cl_assert(!kernel_volatile_malloc(0));
  kernel_volatile_free(NULL);
}

void test_volatile_heap__failed_allocation_keeps_hold(void) {
  void *a = kernel_volatile_malloc(256);
  cl_assert(a);
  cl_assert(!kernel_volatile_malloc(CONFIG_KERNEL_VOLATILE_HEAP_SIZE));
  cl_assert_equal_i(s_holds, 1);
  kernel_volatile_free(a);
}

void test_volatile_heap__rebuilds_after_contents_are_lost(void) {
  void *a = kernel_volatile_malloc(256);
  cl_assert(a);
  kernel_volatile_free(a);
  prv_power_down();

  uint8_t *b = kernel_volatile_zalloc_check(512);
  for (size_t i = 0; i < 512; ++i) {
    cl_assert_equal_i(b[i], 0);
  }
  void *c = kernel_volatile_malloc(1024);
  cl_assert(c);
  kernel_volatile_free(b);
  kernel_volatile_free(c);
}

void test_volatile_heap__failed_realloc_preserves_data_and_hold(void) {
  uint8_t *a = kernel_volatile_malloc(256);
  cl_assert(a);
  memset(a, 0x5a, 256);
  cl_assert(!kernel_volatile_realloc(a, CONFIG_KERNEL_VOLATILE_HEAP_SIZE * 2));
  cl_assert_equal_i(s_holds, 1);
  for (size_t i = 0; i < 256; ++i) {
    cl_assert_equal_i(a[i], 0x5a);
  }
  uint8_t *b = kernel_volatile_realloc(a, 512);
  cl_assert(b);
  for (size_t i = 0; i < 256; ++i) {
    cl_assert_equal_i(b[i], 0x5a);
  }
  cl_assert(!kernel_volatile_realloc(b, 0));
  cl_assert_equal_i(s_holds, 0);
}

void test_volatile_heap__stats(void) {
  unsigned int used, high_water, allocations;
  void *a = kernel_volatile_malloc(1000);
  kernel_volatile_heap_stats(&used, &high_water, &allocations);
  cl_assert(used >= 1000);
  cl_assert(high_water >= 1000);
  cl_assert_equal_i(allocations, 1);
  kernel_volatile_free(a);
  kernel_volatile_heap_stats(&used, &high_water, &allocations);
  cl_assert_equal_i(used, 0);
  cl_assert(high_water >= 1000);
  cl_assert_equal_i(allocations, 0);
}
