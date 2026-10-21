/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/scratch.h>

#ifndef CONFIG_SCRATCH_HEAP_SHARED

#include <pbl/drivers/ram.h>
#include <pbl/kernel/cache.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>

// First fit in granules. Both maps are in internal RAM, so nothing of the heap is kept in the
// memory itself, which is only powered while something is allocated.
#define GRANULE  1024U
#define GRANULES (CONFIG_SCRATCH_HEAP_SIZE / GRANULE)
#define WORDS    ((GRANULES + 31U) / 32U)

static uint32_t s_used[WORDS];
// Last granule of each block
static uint32_t s_last[WORDS];
static size_t s_used_granules;
static size_t s_peak_granules;
static uint32_t s_allocations;
static PBL_MUTEX_DEFINE(s_lock);

static uintptr_t prv_base(void) {
  uintptr_t base;
  size_t size;

  pbl_ram_get_region(EXTRAM, &base, &size);
  return base;
}

static size_t prv_total(void) {
  uintptr_t base;
  size_t size;

  pbl_ram_get_region(EXTRAM, &base, &size);
  return MIN(GRANULES, size / GRANULE);
}

static bool prv_test(const uint32_t *map, size_t i) {
  return (map[i / 32U] & PBL_BIT(i % 32U)) != 0U;
}

static void prv_set(uint32_t *map, size_t i, bool set) {
  if (set) {
    map[i / 32U] |= PBL_BIT(i % 32U);
  } else {
    map[i / 32U] &= ~PBL_BIT(i % 32U);
  }
}

// First free run of n granules, or the largest one with n = 0
static size_t prv_find(size_t n, size_t *largest) {
  const size_t total = prv_total();
  size_t run = 0U;
  size_t i = 0U;

  *largest = 0U;
  while (i < total) {
    if ((i % 32U == 0U) && (s_used[i / 32U] == UINT32_MAX)) {
      run = 0U;
      i += 32U;
      continue;
    }
    run = prv_test(s_used, i) ? 0U : run + 1U;
    *largest = MAX(*largest, run);
    if ((n > 0U) && (run == n)) {
      return i + 1U - n;
    }
    i++;
  }

  return total;
}

void *pbl_scratch_malloc(size_t size) {
  const size_t n = (size + GRANULE - 1U) / GRANULE;
  size_t largest;
  size_t first;

  if ((size == 0U) || (size > CONFIG_SCRATCH_HEAP_SIZE)) {
    return nullptr;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  first = prv_find(n, &largest);
  if ((first == prv_total()) || ((s_allocations == 0U) && (pbl_ram_get(EXTRAM) != 0))) {
    pbl_mutex_unlock(&s_lock);
    return nullptr;
  }

  for (size_t i = first; i < first + n; i++) {
    prv_set(s_used, i, true);
  }
  prv_set(s_last, first + n - 1U, true);
  s_allocations++;
  s_used_granules += n;
  s_peak_granules = MAX(s_peak_granules, s_used_granules);
  pbl_mutex_unlock(&s_lock);

  return (void *)(prv_base() + first * GRANULE);
}

void pbl_scratch_free(void *ptr) {
  const uintptr_t base = prv_base();
  const uintptr_t offset = (uintptr_t)ptr - base;
  const size_t first = offset / GRANULE;
  size_t i = first;
  bool last;

  if (ptr == nullptr) {
    return;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  PBL_ASSERTN(((uintptr_t)ptr >= base) && (offset % GRANULE == 0U) && (first < prv_total()) &&
              prv_test(s_used, first) &&
              ((first == 0U) || !prv_test(s_used, first - 1U) || prv_test(s_last, first - 1U)));
  do {
    last = prv_test(s_last, i);
    prv_set(s_used, i, false);
    prv_set(s_last, i, false);
    i++;
  } while (!last);

  // What it held is gone: nothing of it may be written back later
  pbl_dcache_invalidate_range(ptr, (i - first) * GRANULE);

  s_used_granules -= i - first;
  if (--s_allocations == 0U) {
    pbl_ram_put(EXTRAM);
  }
  pbl_mutex_unlock(&s_lock);
}

void pbl_scratch_get_stats(struct pbl_scratch_stats *stats) {
  size_t largest;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  (void)prv_find(0U, &largest);
  *stats = (struct pbl_scratch_stats){
    .size = prv_total() * GRANULE,
    .used = s_used_granules * GRANULE,
    .peak = s_peak_granules * GRANULE,
    .max_free = largest * GRANULE,
    .allocations = s_allocations,
  };
  pbl_mutex_unlock(&s_lock);
}

#else

#include <pbl/kernel/heap.h>
#include <pbl/kernel/malloc.h>

void *pbl_scratch_malloc(size_t size) {
  return pbl_malloc(size);
}

void pbl_scratch_free(void *ptr) {
  pbl_free(ptr);
}

void pbl_scratch_get_stats(struct pbl_scratch_stats *stats) {
  struct pbl_heap *heap = pbl_malloc_heap();
  unsigned int used;
  unsigned int free_bytes;
  unsigned int max_free;

  pbl_heap_stats(heap, &used, &free_bytes, &max_free);
  *stats = (struct pbl_scratch_stats){
    .size = pbl_heap_size(heap),
    .used = used,
    .max_free = max_free,
  };
}

#endif

void *pbl_scratch_zalloc(size_t size) {
  void *ptr = pbl_scratch_malloc(size);

  if (ptr != nullptr) {
    memset(ptr, 0, size);
  }

  return ptr;
}

void *pbl_scratch_zalloc_check(size_t size) {
  void *ptr = pbl_scratch_zalloc(size);

  PBL_ASSERTN(ptr != nullptr);

  return ptr;
}
