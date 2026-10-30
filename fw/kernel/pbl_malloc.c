/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl_malloc.h"

#include "pebble_tasks.h"

#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/heap.h>
#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>

#include <process_management/app_manager.h>
#include <process_state/app_state/app_state.h>
#include <process_state/worker_state/worker_state.h>

struct pbl_heap *task_heap_get_for_current_task(void) {
  if (pebble_task_get_current() == PebbleTask_App) {
    return app_state_get_heap();
  } else if (pebble_task_get_current() == PebbleTask_Worker) {
    return worker_state_get_heap();
  }
  return pbl_malloc_heap();
}

// task_* functions that map to other heaps depending on the current task
///////////////////////////////////////////////////////////
#if defined(CONFIG_MALLOC_INSTRUMENTATION)
void *task_malloc_with_pc(size_t bytes, uintptr_t client_pc) {
  return pbl_heap_alloc(task_heap_get_for_current_task(), bytes, client_pc);
}
#endif

void *task_malloc(size_t bytes) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_alloc(task_heap_get_for_current_task(), bytes, saved_lr);
}

void *task_malloc_check(size_t bytes) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  struct pbl_heap *heap = task_heap_get_for_current_task();
  void *mem = pbl_heap_alloc(heap, bytes, saved_lr);

  if (!mem && bytes != 0) {
    pbl_heap_oom(heap, bytes, saved_lr);
  }
  return mem;
}

#if defined(CONFIG_MALLOC_INSTRUMENTATION)
void task_free_with_pc(void *ptr, uintptr_t client_pc) {
  pbl_heap_free(task_heap_get_for_current_task(), ptr, client_pc);
}
#endif

void task_free(void *ptr) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  pbl_heap_free(task_heap_get_for_current_task(), ptr, saved_lr);
}

void *task_realloc(void *ptr, size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_realloc(task_heap_get_for_current_task(), ptr, size, saved_lr);
}

void *task_zalloc(size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_zalloc(task_heap_get_for_current_task(), size, saved_lr);
}

void *task_zalloc_check(size_t bytes) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  struct pbl_heap *heap = task_heap_get_for_current_task();
  void *mem = pbl_heap_zalloc(heap, bytes, saved_lr);

  if (!mem && bytes != 0) {
    pbl_heap_oom(heap, bytes, saved_lr);
  }
  return mem;
}

void *task_calloc(size_t count, size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_calloc(task_heap_get_for_current_task(), count, size, saved_lr);
}

void *task_calloc_check(size_t count, size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  struct pbl_heap *heap = task_heap_get_for_current_task();
  void *mem = pbl_heap_calloc(heap, count, size, saved_lr);

  const size_t bytes = count * size;
  if (!mem && bytes != 0) {
    pbl_heap_oom(heap, bytes, saved_lr);
  }
  return mem;
}

char *task_strdup(const char *s) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_strdup(task_heap_get_for_current_task(), s, saved_lr);
}

// Wrappers (Jay-Z, Tupac, etc)
// We want to keep these around for code bases that we don't own. For example, libc will want
// malloc to exist, and libc should use the appropriate heap based on the task.
////////////////////////////////////////////////////////////
void *__wrap_malloc(size_t bytes) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_alloc(task_heap_get_for_current_task(), bytes, saved_lr);
}

void __wrap_free(void *ptr) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  pbl_heap_free(task_heap_get_for_current_task(), ptr, saved_lr);
}

void *__wrap_realloc(void *ptr, size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_realloc(task_heap_get_for_current_task(), ptr, size, saved_lr);
}

void *__wrap_calloc(size_t count, size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_calloc(task_heap_get_for_current_task(), count, size, saved_lr);
}
