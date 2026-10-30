/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "volatile_heap.h"

#ifdef CONFIG_KERNEL_VOLATILE_HEAP

#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/psram.h>
#include <pbl/kernel/section.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/util/heap.h>
#include <pbl/util/math.h>
#include <pbl/util/testing.h>

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>
#endif

PBL_T_STATIC uint8_t
    s_storage[CONFIG_KERNEL_VOLATILE_HEAP_SIZE] PBL_SECTION_DISPOSABLE PBL_ALIGNED(32);

// Control state stays in SRAM, so it survives the loss of the storage
static Heap s_heap;
static unsigned int s_allocations;
static unsigned int s_high_water;
static PBL_MUTEX_DEFINE(s_lock);

static void prv_track_high_water(void) {
  if (s_heap.high_water_mark > s_high_water) {
    s_high_water = s_heap.high_water_mark;
    PBL_ANALYTICS_SET_UNSIGNED(volatile_heap_high_water, s_high_water);
  }
}

void *kernel_volatile_malloc(size_t size) {
  void *ptr;

  if (size == 0U) {
    return NULL;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_allocations == 0U) {
    psram_retain_acquire();
    // The headers may have been lost while the heap was empty
    heap_init(&s_heap, s_storage, s_storage + sizeof(s_storage), false);
  }

  ptr = heap_malloc(&s_heap, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
  if (ptr != NULL) {
    s_allocations++;
    prv_track_high_water();
  } else if (s_allocations == 0U) {
    psram_retain_release();
  }
  pbl_mutex_unlock(&s_lock);

  return ptr;
}

void *kernel_volatile_zalloc_check(size_t size) {
  void *ptr = kernel_volatile_malloc(size);

  PBL_ASSERTN(ptr != NULL);
  memset(ptr, 0, size);

  return ptr;
}

void *kernel_volatile_realloc(void *ptr, size_t size) {
  void *result;

  if (ptr == NULL) {
    return kernel_volatile_malloc(size);
  }

  if (size == 0U) {
    kernel_volatile_free(ptr);
    return NULL;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  PBL_ASSERTN((s_allocations > 0U) && heap_contains_address(&s_heap, ptr));
  result = heap_realloc(&s_heap, ptr, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
  prv_track_high_water();
  pbl_mutex_unlock(&s_lock);

  return result;
}

void kernel_volatile_free(void *ptr) {
  if (ptr == NULL) {
    return;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  PBL_ASSERTN((s_allocations > 0U) && heap_contains_address(&s_heap, ptr));
  heap_free(&s_heap, ptr, (uintptr_t)PBL_RETURN_ADDRESS(0));
  if (--s_allocations == 0U) {
    psram_retain_release();
  }
  pbl_mutex_unlock(&s_lock);
}

void kernel_volatile_heap_stats(unsigned int *used, unsigned int *high_water,
                                unsigned int *allocations) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  *used = (s_allocations > 0U) ? s_heap.current_size : 0U;
  *high_water = s_high_water;
  *allocations = s_allocations;
  pbl_mutex_unlock(&s_lock);
}

#ifdef CONFIG_SHELL
static int prv_cmd_volatile_heap(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned int used, high_water, allocations;

  kernel_volatile_heap_stats(&used, &high_water, &allocations);
  pbl_shell_print(sh, "Used: %u of %u bytes, peak %u, %u allocations", used,
                  (unsigned int)CONFIG_KERNEL_VOLATILE_HEAP_SIZE, high_water, allocations);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, volatile_heap, nullptr, "Show the volatile kernel heap usage",
                     prv_cmd_volatile_heap, 0, 0);
#endif

#endif
