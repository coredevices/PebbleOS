/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

/**
 * @defgroup kernel_scratch Scratch heap
 * @ingroup kernel
 * @brief Kernel heap for short-lived data.
 *
 * A heap apart from the kernel's own (see @ref kernel_malloc), for buffers that only live for
 * the length of an operation. Where it is and what it costs is the platform's business: it may
 * be memory that is only powered while something is allocated, so what is allocated here should
 * be freed as soon as it is no longer needed, and nothing should be kept for long.
 *
 * @code{.c}
 * uint8_t *buf = pbl_scratch_malloc(len);
 * if (buf == NULL) {
 *   return -ENOMEM;
 * }
 * prv_process(buf, len);
 * pbl_scratch_free(buf);
 * @endcode
 * @{
 */

/**
 * @brief Allocate from the scratch heap.
 *
 * The memory is aligned for any type. Not callable from interrupts.
 *
 * @param size Size, in bytes.
 * @return The memory, or NULL if there is no room.
 */
void *pbl_scratch_malloc(size_t size);

/**
 * @brief Allocate zeroed memory from the scratch heap.
 *
 * @param size Size, in bytes.
 * @return The memory, or NULL if there is no room.
 */
void *pbl_scratch_zalloc(size_t size);

/**
 * @brief Allocate zeroed memory from the scratch heap, asserting on failure.
 *
 * @param size Size, in bytes.
 * @return The memory.
 */
void *pbl_scratch_zalloc_check(size_t size);

/**
 * @brief Free memory allocated from the scratch heap.
 *
 * @param ptr Memory, or NULL.
 */
void pbl_scratch_free(void *ptr);

/** @brief Usage of the scratch heap. */
struct pbl_scratch_stats {
  /** Size of the heap, in bytes. */
  size_t size;
  /** Bytes allocated. */
  size_t used;
  /** Most bytes ever allocated at once, 0 if not tracked. */
  size_t peak;
  /** Largest block that can be allocated now. */
  size_t max_free;
  /** Live allocations, 0 if not tracked. */
  unsigned int allocations;
};

/**
 * @brief Get the usage of the scratch heap.
 *
 * @param[out] stats Usage.
 */
void pbl_scratch_get_stats(struct pbl_scratch_stats *stats);

/** @} */
