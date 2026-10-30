/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>
#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/heap.h>
#include <pbl/kernel/malloc.h>

PBL_WEAK void *app_malloc(size_t bytes) {
  return malloc(bytes);
}

PBL_WEAK void *app_malloc_check(size_t bytes) {
  return malloc(bytes);
}

PBL_WEAK void *app_zalloc(size_t bytes) {
  void *ptr = app_malloc(bytes);
  memset(ptr, 0, bytes);
  return ptr;
}

PBL_WEAK void *app_zalloc_check(size_t bytes) {
  return app_zalloc(bytes);
}

PBL_WEAK void *app_calloc(size_t count, size_t size) {
  return calloc(count, size);
}

PBL_WEAK void *app_calloc_check(size_t count, size_t size) {
  return app_calloc(count, size);
}

PBL_WEAK void app_free(void *ptr) {
  free(ptr);
}

// Set to make pbl_malloc/pbl_zalloc/pbl_strdup return NULL, for OOM testing.
static bool s_pbl_malloc_should_fail = false;

PBL_WEAK void stub_pbl_malloc_set_should_fail(bool should_fail) {
  s_pbl_malloc_should_fail = should_fail;
}

PBL_WEAK void smart_free(void *ptr) {
  free(ptr);
}

PBL_WEAK [[noreturn]] void pbl_heap_oom(struct pbl_heap *heap, size_t size, uintptr_t caller) {
  abort();
}

static struct pbl_heap s_heap;
PBL_WEAK struct pbl_heap *pbl_malloc_heap(void) {
  return &s_heap;
}

PBL_WEAK void *pbl_malloc(size_t size) {
  if (s_pbl_malloc_should_fail) {
    return nullptr;
  }
  return malloc(size);
}

PBL_WEAK void *pbl_malloc_check(size_t size) {
  return malloc(size);
}

PBL_WEAK void *pbl_zalloc(size_t size) {
  if (s_pbl_malloc_should_fail) {
    return nullptr;
  }
  return calloc(1, size);
}

PBL_WEAK void *pbl_zalloc_check(size_t size) {
  return calloc(1, size);
}

PBL_WEAK void *pbl_calloc(size_t count, size_t size) {
  return calloc(count, size);
}

PBL_WEAK void *pbl_calloc_check(size_t count, size_t size) {
  return calloc(count, size);
}

PBL_WEAK void *pbl_realloc(void *ptr, size_t size) {
  return realloc(ptr, size);
}

PBL_WEAK char *pbl_strdup(const char *s) {
  if (s_pbl_malloc_should_fail) {
    return nullptr;
  }
  return strdup(s);
}

PBL_WEAK char *pbl_strdup_check(const char *s) {
  return strdup(s);
}

PBL_WEAK void pbl_free(void *ptr) {
  free(ptr);
}
