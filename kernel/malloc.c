/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel.h"

#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/heap.h>
#include <pbl/kernel/malloc.h>

static struct pbl_heap s_kernel_heap;

static void prv_lock(void *ctx) {
  arch_heap_lock();
}

static void prv_unlock(void *ctx) {
  arch_heap_unlock();
}

void pbl_malloc_init(void) {
  extern int _heap_start;
  extern int _heap_end;

  pbl_heap_init(&s_kernel_heap, &_heap_start, &_heap_end, true);
  pbl_heap_set_lock(&s_kernel_heap, (struct pbl_heap_lock){.lock = prv_lock, .unlock = prv_unlock});
}

struct pbl_heap *pbl_malloc_heap(void) {
  return &s_kernel_heap;
}

static void *prv_check(void *mem, size_t size, uintptr_t caller) {
  if (!mem && size != 0) {
    pbl_heap_oom(&s_kernel_heap, size, caller);
  }
  return mem;
}

void *pbl_malloc(size_t size) {
  return pbl_heap_alloc(&s_kernel_heap, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

void *pbl_malloc_check(size_t size) {
  uintptr_t caller = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return prv_check(pbl_heap_alloc(&s_kernel_heap, size, caller), size, caller);
}

void *pbl_zalloc(size_t size) {
  return pbl_heap_zalloc(&s_kernel_heap, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

void *pbl_zalloc_check(size_t size) {
  uintptr_t caller = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return prv_check(pbl_heap_zalloc(&s_kernel_heap, size, caller), size, caller);
}

void *pbl_calloc(size_t count, size_t size) {
  return pbl_heap_calloc(&s_kernel_heap, count, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

void *pbl_calloc_check(size_t count, size_t size) {
  uintptr_t caller = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return prv_check(pbl_heap_calloc(&s_kernel_heap, count, size, caller), count * size, caller);
}

void *pbl_realloc(void *ptr, size_t size) {
  return pbl_heap_realloc(&s_kernel_heap, ptr, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

char *pbl_strdup(const char *s) {
  return pbl_heap_strdup(&s_kernel_heap, s, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

char *pbl_strdup_check(const char *s) {
  uintptr_t caller = (uintptr_t)PBL_RETURN_ADDRESS(0);

  return prv_check(pbl_heap_strdup(&s_kernel_heap, s, caller), strlen(s) + 1, caller);
}

void pbl_free(void *ptr) {
  pbl_heap_free(&s_kernel_heap, ptr, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

// The firmware links with --wrap for these, so code that calls libc directly, mostly third party,
// allocates from the kernel heap.
void *__wrap_malloc(size_t size) {
  return pbl_heap_alloc(&s_kernel_heap, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

void *__wrap_calloc(size_t count, size_t size) {
  return pbl_heap_calloc(&s_kernel_heap, count, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

void *__wrap_realloc(void *ptr, size_t size) {
  return pbl_heap_realloc(&s_kernel_heap, ptr, size, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

void __wrap_free(void *ptr) {
  pbl_heap_free(&s_kernel_heap, ptr, (uintptr_t)PBL_RETURN_ADDRESS(0));
}
