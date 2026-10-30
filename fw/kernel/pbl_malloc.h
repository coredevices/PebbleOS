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

struct pbl_heap;

struct pbl_heap *task_heap_get_for_current_task(void);

// task_* functions map to app_* or kernel_* based on which task we're calling it on.
void *task_malloc(size_t bytes);
#if defined(CONFIG_MALLOC_INSTRUMENTATION)
void *task_malloc_with_pc(size_t bytes, uintptr_t client_pc);
#endif
void *task_malloc_check(size_t bytes);
void *task_realloc(void *ptr, size_t size);
void *task_zalloc(size_t size);
void *task_zalloc_check(size_t size);
void *task_calloc(size_t count, size_t size);
void *task_calloc_check(size_t count, size_t size);
void task_free(void *ptr);
#if defined(CONFIG_MALLOC_INSTRUMENTATION)
void task_free_with_pc(void *ptr, uintptr_t client_pc);
#endif
char *task_strdup(const char *s);
