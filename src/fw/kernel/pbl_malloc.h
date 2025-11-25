/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stddef.h>

//! @file pbl_malloc.h
//!
//! In the firmware, we actually have multiple heaps that clients can use to malloc/free on.
//! The kernel heap is located in protected memory and is used by the kernel itself. The app
//! heap is located inside the app region and is reset between each app.

typedef struct Heap Heap;

Heap *task_heap_get_for_current_task(void);

// task_* functions map to app_* or kernel_* based on which task we're calling it on.
void *task_malloc(size_t bytes);
#if defined(MALLOC_INSTRUMENTATION)
void *task_malloc_with_pc(size_t bytes, uintptr_t client_pc);
#endif
void *task_malloc_check(size_t bytes);
void *task_realloc(void *ptr, size_t size);
void *task_zalloc(size_t size);
void *task_zalloc_check(size_t size);
void *task_calloc(size_t count, size_t size);
void *task_calloc_check(size_t count, size_t size);
void task_free(void *ptr);
#if defined(MALLOC_INSTRUMENTATION)
void task_free_with_pc(void *ptr, uintptr_t client_pc);
#endif
char* task_strdup(const char* s);

void *app_malloc(size_t bytes);
void *app_malloc_check(size_t bytes);
void *app_realloc(void *ptr, size_t bytes);
void *app_zalloc(size_t size);
void *app_zalloc_check(size_t size);
void *app_calloc(size_t count, size_t size);
void *app_calloc_check(size_t count, size_t size);
void app_free(void *ptr);
char* app_strdup(const char* s);

void *kernel_malloc(size_t bytes);
void *kernel_malloc_check(size_t bytes);
void *kernel_realloc(void *ptr, size_t bytes);
void *kernel_zalloc(size_t size);
void *kernel_zalloc_check(size_t size);
void *kernel_calloc(size_t count, size_t size);
void *kernel_calloc_check(size_t count, size_t size);
void kernel_free(void *ptr);
char *kernel_strdup(const char *s);
char *kernel_strdup_check(const char *s);
