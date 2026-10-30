/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <pbl/kernel/heap.h>

/**
 * @defgroup kernel_malloc Kernel heap
 * @ingroup kernel
 * @brief Allocations from the kernel heap.
 *
 * The kernel heap sits in kernel RAM, between the linker symbols @c _heap_start and
 * @c _heap_end. Every function here allocates from it, whatever thread calls it, and can be used
 * before the scheduler starts and from interrupts that may call the kernel. Other heaps are
 * reached through their own @ref kernel_heap "struct pbl_heap".
 *
 * The @c _check variants never return NULL: they call pbl_heap_oom() instead.
 *
 * @code{.c}
 * struct entry *e = pbl_zalloc(sizeof(*e));
 * if (e == NULL) {
 *   return -ENOMEM;
 * }
 * ...
 * pbl_free(e);
 * @endcode
 * @{
 */

/** @brief Initialize the kernel heap. Called once at boot, before any allocation. */
void pbl_malloc_init(void);

/**
 * @brief Get the kernel heap.
 *
 * @return Kernel heap.
 */
struct pbl_heap *pbl_malloc_heap(void);

/**
 * @brief Allocate memory.
 *
 * @param size Number of bytes.
 * @return Allocated memory, or NULL.
 */
void *pbl_malloc(size_t size);

/**
 * @brief Allocate memory, or fail with pbl_heap_oom().
 *
 * @param size Number of bytes.
 * @return Allocated memory; NULL only if @p size is 0.
 */
void *pbl_malloc_check(size_t size);

/**
 * @brief Allocate zeroed memory.
 *
 * @param size Number of bytes.
 * @return Allocated memory, or NULL.
 */
void *pbl_zalloc(size_t size);

/**
 * @brief Allocate zeroed memory, or fail with pbl_heap_oom().
 *
 * @param size Number of bytes.
 * @return Allocated memory; NULL only if @p size is 0.
 */
void *pbl_zalloc_check(size_t size);

/**
 * @brief Allocate a zeroed array.
 *
 * @param count Number of elements.
 * @param size Size of an element in bytes.
 * @return Allocated memory, or NULL.
 */
void *pbl_calloc(size_t count, size_t size);

/**
 * @brief Allocate a zeroed array, or fail with pbl_heap_oom().
 *
 * @param count Number of elements.
 * @param size Size of an element in bytes.
 * @return Allocated memory; NULL only if the total size is 0.
 */
void *pbl_calloc_check(size_t count, size_t size);

/**
 * @brief Reallocate memory.
 *
 * @param ptr Memory from the kernel heap, or NULL.
 * @param size New size in bytes.
 * @return New memory, or NULL on failure (@p ptr is then left untouched).
 */
void *pbl_realloc(void *ptr, size_t size);

/**
 * @brief Duplicate a string.
 *
 * @param s String.
 * @return Copy of @p s, or NULL.
 */
char *pbl_strdup(const char *s);

/**
 * @brief Duplicate a string, or fail with pbl_heap_oom().
 *
 * @param s String.
 * @return Copy of @p s.
 */
char *pbl_strdup_check(const char *s);

/**
 * @brief Free memory.
 *
 * @param ptr Memory from the kernel heap, or NULL (no-op). Anything else is a fatal error.
 */
void pbl_free(void *ptr);

/** @} */
