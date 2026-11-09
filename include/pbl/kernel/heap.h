/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup kernel_heap Heaps
 * @ingroup kernel
 * @brief First-fit heaps over caller-provided memory regions.
 *
 * Small allocations are taken from the start of the region and large ones (256 bytes or more)
 * from the end, to limit fragmentation. Each block carries a small header and, with
 * @c CONFIG_MALLOC_INSTRUMENTATION, the address of the caller that allocated it. A heap is not
 * thread safe unless given a lock with pbl_heap_set_lock().
 *
 * The kernel heap is behind pbl_malloc(); other heaps belong to whoever initializes them.
 *
 * @code{.c}
 * static uint8_t s_region[4096];
 * static struct pbl_heap s_heap;
 *
 * pbl_heap_init(&s_heap, s_region, s_region + sizeof(s_region), false);
 * void *p = pbl_heap_alloc(&s_heap, 100, (uintptr_t)PBL_RETURN_ADDRESS(0));
 * pbl_heap_free(&s_heap, p, 0);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct _tagHeapInfo_t;
typedef struct _tagHeapInfo_t HeapInfo_t;
/** @endcond */

/** @brief Locking of a heap. */
struct pbl_heap_lock {
  /** Takes the lock, called with @ref ctx. */
  void (*lock)(void *ctx);
  /** Releases the lock, called with @ref ctx. */
  void (*unlock)(void *ctx);
  /** Context passed to both functions. */
  void *ctx;
};

/**
 * @brief Handler of a heap error, called with the pointer being freed (double free) or the block
 * where corruption was found.
 *
 * Called with the heap unlocked.
 */
typedef void (*pbl_heap_error_handler_t)(void *ptr);

/** @brief Heap. */
struct pbl_heap {
  /** First block of the heap. */
  HeapInfo_t *begin;
  /** End of the heap, right after the last block. */
  HeapInfo_t *end;

  /** Number of allocated bytes, including block headers. */
  unsigned int current_size;
  /** Peak of @ref current_size. */
  unsigned int high_water_mark;

  /** Locking. */
  struct pbl_heap_lock lock;

  /** Double free handler, NULL to assert. */
  pbl_heap_error_handler_t double_free_handler;
  /** Fill freed memory with a junk pattern (non-release builds). */
  bool fuzz_on_free;

  /** Corrupted block found while locked, reported once unlocked. */
  void *corrupt_block;
  /** Corruption handler, NULL to assert. */
  pbl_heap_error_handler_t corruption_handler;
};

/**
 * @brief Initialize a heap over a memory region, zeroing it.
 *
 * The usable size is capped to about 32767 alignment units (128 KiB with 4-byte units).
 *
 * @param[out] heap Heap.
 * @param start Start of the region, rounded up to the alignment.
 * @param end End of the region, exclusive, rounded down to the alignment.
 * @param fuzz_on_free Fill freed memory with a junk pattern to catch use after free sooner.
 */
void pbl_heap_init(struct pbl_heap *heap, void *start, void *end, bool fuzz_on_free);

/**
 * @brief Make a heap thread safe with a lock.
 *
 * @param heap Heap.
 * @param lock Lock functions; either may be NULL.
 */
void pbl_heap_set_lock(struct pbl_heap *heap, struct pbl_heap_lock lock);

/**
 * @brief Set the function called on a double free, instead of asserting.
 *
 * @param heap Heap.
 * @param handler Handler; the free is then ignored.
 */
void pbl_heap_set_double_free_handler(struct pbl_heap *heap, pbl_heap_error_handler_t handler);

/**
 * @brief Set the function called when corruption is detected, instead of asserting.
 *
 * @param heap Heap.
 * @param handler Handler.
 */
void pbl_heap_set_corruption_handler(struct pbl_heap *heap, pbl_heap_error_handler_t handler);

/**
 * @brief Allocate memory.
 *
 * @param heap Heap, initialized.
 * @param size Number of bytes, greater than 0.
 * @param caller Caller address, recorded with @c CONFIG_MALLOC_INSTRUMENTATION.
 * @return Allocated memory, or NULL if there is not enough contiguous free space.
 */
void *pbl_heap_alloc(struct pbl_heap *heap, size_t size, uintptr_t caller);

/**
 * @brief Allocate zeroed memory.
 *
 * @param heap Heap.
 * @param size Number of bytes.
 * @param caller Caller address, for instrumentation.
 * @return Allocated memory, or NULL.
 */
void *pbl_heap_zalloc(struct pbl_heap *heap, size_t size, uintptr_t caller);

/**
 * @brief Allocate a zeroed array.
 *
 * @param heap Heap.
 * @param count Number of elements.
 * @param size Size of an element in bytes.
 * @param caller Caller address, for instrumentation.
 * @return Allocated memory, or NULL if there is not enough space or @p count times @p size
 * overflows.
 */
void *pbl_heap_calloc(struct pbl_heap *heap, size_t count, size_t size, uintptr_t caller);

/**
 * @brief Reallocate memory.
 *
 * Always allocates a new block, copies as much of the old data as fits, then frees @p ptr.
 *
 * @param heap Heap.
 * @param ptr Memory to reallocate, or NULL.
 * @param size New size in bytes.
 * @param caller Caller address, recorded with @c CONFIG_MALLOC_INSTRUMENTATION.
 * @return New memory, or NULL on failure (@p ptr is then left untouched).
 */
void *pbl_heap_realloc(struct pbl_heap *heap, void *ptr, size_t size, uintptr_t caller);

/**
 * @brief Duplicate a string.
 *
 * @param heap Heap.
 * @param s String.
 * @param caller Caller address, for instrumentation.
 * @return Copy of @p s, or NULL.
 */
char *pbl_heap_strdup(struct pbl_heap *heap, const char *s, uintptr_t caller);

/**
 * @brief Free memory, merging it with adjacent free blocks.
 *
 * @param heap Heap, initialized.
 * @param ptr Memory allocated from @p heap, or NULL (no-op).
 * @param caller Caller address, for instrumentation.
 */
void pbl_heap_free(struct pbl_heap *heap, void *ptr, uintptr_t caller);

/**
 * @brief Report an allocation failure the caller cannot recover from.
 *
 * Logs the heap usage, dumps the heap with @c CONFIG_MALLOC_INSTRUMENTATION and calls
 * pbl_heap_oom_handler().
 *
 * @param heap Heap the allocation failed on.
 * @param size Number of bytes requested.
 * @param caller Caller address.
 */
[[noreturn]] void pbl_heap_oom(struct pbl_heap *heap, size_t size, uintptr_t caller);

/**
 * @brief Handle an allocation failure the caller cannot recover from.
 *
 * Called by pbl_heap_oom(). The default ends in pbl_fatal_error_handler(); an application
 * overrides it, e.g. to only kill the process that owns the heap.
 *
 * @param heap Heap the allocation failed on.
 * @param size Number of bytes requested.
 * @param caller Caller address.
 */
[[noreturn]] void pbl_heap_oom_handler(struct pbl_heap *heap, size_t size, uintptr_t caller);

/**
 * @brief Check whether an address is within a heap.
 *
 * @param heap Heap.
 * @param ptr Address.
 * @return true if @p ptr is in the heap region.
 */
bool pbl_heap_contains(const struct pbl_heap *heap, const void *ptr);

/**
 * @brief Check whether a pointer is an allocated block of a heap.
 *
 * @param heap Heap.
 * @param ptr Pointer.
 * @return true if @p ptr was returned by an allocation and not freed.
 */
bool pbl_heap_is_allocated(struct pbl_heap *heap, void *ptr);

/**
 * @brief Get the size of a heap.
 *
 * @param heap Heap.
 * @return Size in bytes.
 */
size_t pbl_heap_size(const struct pbl_heap *heap);

/**
 * @brief Get the number of allocated bytes, including block headers.
 *
 * @param heap Heap.
 * @return Allocated bytes.
 */
size_t pbl_heap_used(const struct pbl_heap *heap);

/**
 * @brief Get the smallest amount of free memory the heap has had.
 *
 * @param heap Heap.
 * @return Heap size minus the high water mark, in bytes.
 */
size_t pbl_heap_min_headroom(const struct pbl_heap *heap);

/**
 * @brief Restart the high water mark from the current usage.
 *
 * @param heap Heap.
 */
void pbl_heap_reset_high_water_mark(struct pbl_heap *heap);

/**
 * @brief Walk a heap to calculate its usage.
 *
 * @param heap Heap.
 * @param[out] used Bytes allocated.
 * @param[out] free Bytes free.
 * @param[out] max_free Size of the largest free block.
 */
void pbl_heap_stats(struct pbl_heap *heap, unsigned int *used, unsigned int *free,
                    unsigned int *max_free);

/**
 * @brief Receives one line of a heap dump, with the heap locked.
 *
 * @param line Line, without line terminator.
 * @param arg Argument given to pbl_heap_dump().
 */
typedef void (*pbl_heap_dump_cb_t)(const char *line, void *arg);

/**
 * @brief Dump every block and the heap totals, one line at a time.
 *
 * Only available with @c CONFIG_MALLOC_INSTRUMENTATION.
 *
 * @param heap Heap.
 * @param cb Called for each line.
 * @param arg Passed to @p cb.
 */
void pbl_heap_dump(struct pbl_heap *heap, pbl_heap_dump_cb_t cb, void *arg);

/** @} */
