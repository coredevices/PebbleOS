/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_malloc.h"

#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/heap.h>

#include <process_state/app_state/app_state.h>

void *app_malloc(size_t bytes) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_alloc(app_state_get_heap(), bytes, saved_lr);
}

void *app_malloc_check(size_t bytes) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  struct pbl_heap *heap = app_state_get_heap();
  void *mem = pbl_heap_alloc(heap, bytes, saved_lr);
  if (!mem && bytes != 0) {
    pbl_heap_oom(heap, bytes, saved_lr);
  }
  return mem;
}

void app_free(void *ptr) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  pbl_heap_free(app_state_get_heap(), ptr, saved_lr);
}

void *app_realloc(void *ptr, size_t bytes) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_realloc(app_state_get_heap(), ptr, bytes, saved_lr);
}

void *app_zalloc(size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_zalloc(app_state_get_heap(), size, saved_lr);
}

void *app_zalloc_check(size_t bytes) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  struct pbl_heap *heap = app_state_get_heap();
  void *mem = pbl_heap_zalloc(heap, bytes, saved_lr);

  if (!mem && bytes != 0) {
    pbl_heap_oom(heap, bytes, saved_lr);
  }
  return mem;
}

void *app_calloc(size_t count, size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return (pbl_heap_calloc(app_state_get_heap(), count, size, saved_lr));
}

void *app_calloc_check(size_t count, size_t size) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  struct pbl_heap *heap = app_state_get_heap();
  void *mem = pbl_heap_calloc(heap, count, size, saved_lr);

  const size_t bytes = count * size;
  if (!mem && bytes != 0) {
    pbl_heap_oom(heap, bytes, saved_lr);
  }
  return mem;
}

char *app_strdup(const char *s) {
  uintptr_t saved_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return pbl_heap_strdup(app_state_get_heap(), s, saved_lr);
}
