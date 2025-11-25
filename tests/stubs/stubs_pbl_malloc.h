/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdlib.h>
#include <string.h>

#include <util/heap.h>

static Heap s_heap;
Heap *task_heap_get_for_current_task(void) {
  return &s_heap;
}

void *task_malloc(size_t bytes) {
  return malloc(bytes);
}

void *task_realloc(void *ptr, size_t bytes) {
  return realloc(ptr, bytes);
}

void *task_malloc_check(size_t bytes) {
  return malloc(bytes);
}

void *task_zalloc(size_t bytes) {
  void *ptr = task_malloc(bytes);
  memset(ptr, 0, bytes);
  return ptr;
}

void *task_zalloc_check(size_t bytes) {
  return task_zalloc(bytes);
}

void task_free(void *ptr) {
  free(ptr);
}

void* task_calloc(size_t count, size_t size) {
  return calloc(count, size);
}

void* task_calloc_check(size_t count, size_t size) {
  return task_calloc(count, size);
}

void *app_malloc(size_t bytes) {
  return malloc(bytes);
}

void *app_malloc_check(size_t bytes) {
  return malloc(bytes);
}

void *app_zalloc(size_t bytes) {
  void *ptr = app_malloc(bytes);
  memset(ptr, 0, bytes);
  return ptr;
}

void *app_zalloc_check(size_t bytes) {
  return app_zalloc(bytes);
}

void *app_calloc(size_t count, size_t size) {
  return calloc(count, size);
}

void *app_calloc_check(size_t count, size_t size) {
  return app_calloc(count, size);
}

void app_free(void *ptr) {
  free(ptr);
}

void *kernel_malloc(size_t bytes) {
  return malloc(bytes);
}

void *kernel_malloc_check(size_t bytes) {
  return malloc(bytes);
}

void *kernel_zalloc(size_t bytes) {
  void *ptr = kernel_malloc(bytes);
  memset(ptr, 0, bytes);
  return ptr;
}

void *kernel_zalloc_check(size_t bytes) {
  return kernel_zalloc(bytes);
}

void *kernel_realloc(void *ptr, size_t bytes) {
  return realloc(ptr, bytes);
}

void kernel_free(void *ptr) {
  free(ptr);
}

void* kernel_calloc(size_t count, size_t size) {
  return calloc(count, size);
}

void* kernel_calloc_check(size_t count, size_t size) {
  return kernel_calloc(count, size);
}

char* kernel_strdup(const char* s) {
  char *r = malloc(strlen(s) + 1);
  if (!r) {
    return NULL;
  }

  strcpy(r, s);
  return r;
}

char* kernel_strdup_check(const char* s) {
  return kernel_strdup(s);
}

char* task_strdup(const char* s) {
  return kernel_strdup(s);
}

void smart_free(void *ptr) {
  free(ptr);
}
