/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_heap Heap
 * @ingroup util
 * @brief First-fit heap allocator over a caller-provided memory region.
 *
 * Small allocations are taken from the start of the region and large ones (256 bytes or more)
 * from the end, to limit fragmentation. Each block carries a small header. The heap is not thread
 * safe unless given a lock with heap_set_lock_impl().
 *
 * @code{.c}
 * static uint8_t s_region[4096];
 * static Heap s_heap;
 *
 * heap_init(&s_heap, s_region, s_region + sizeof(s_region), false);
 * void *p = heap_malloc(&s_heap, 100, (uintptr_t)__builtin_return_address(0));
 * heap_free(&s_heap, p, 0);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct _tagHeapInfo_t;
typedef struct _tagHeapInfo_t HeapInfo_t;
/** @endcond */

/** @brief Lock function of a heap, called with HeapLockImpl::lock_context. */
typedef void (*LockFunction)(void *);
/** @brief Unlock function of a heap, called with HeapLockImpl::lock_context. */
typedef void (*UnlockFunction)(void *);

/** @brief Locking of a heap. */
typedef struct HeapLockImpl {
  /** Takes the lock. */
  LockFunction lock_function;
  /** Releases the lock. */
  UnlockFunction unlock_function;
  /** Context passed to both functions. */
  void *lock_context;
} HeapLockImpl;

/** @brief Handler of a double free, called with the pointer being freed. */
typedef void (*DoubleFreeHandler)(void *);
/**
 * @brief Handler of a corrupted heap, called with the block where corruption was found.
 *
 * Called after the heap lock is released.
 */
typedef void (*CorruptionHandler)(void *);

/** @brief Heap state. */
typedef struct Heap {
  /** First block of the heap. */
  HeapInfo_t *begin;
  /** End of the heap, right after the last block. */
  HeapInfo_t *end;

  /** Number of allocated bytes, including block headers. */
  unsigned int current_size;
  /** Peak of @ref current_size. */
  unsigned int high_water_mark;

  /** Locking. */
  HeapLockImpl lock_impl;

  /** Double free handler, NULL to assert. */
  DoubleFreeHandler double_free_handler;
  /** Fill freed memory with a junk pattern (non-release builds). */
  bool fuzz_on_free;

  /** Corrupted block found while locked, reported once unlocked. */
  void *corrupt_block;
  /** Corruption handler, NULL to assert. */
  CorruptionHandler corruption_handler;
} Heap;

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
void heap_init(Heap *const heap, void *start, void *end, bool fuzz_on_free);

/**
 * @brief Make a heap thread safe with a lock.
 *
 * @param heap Heap.
 * @param lock_impl Lock functions.
 */
void heap_set_lock_impl(Heap *heap, HeapLockImpl lock_impl);

/**
 * @brief Set the function called on a double free, instead of asserting.
 *
 * @param heap Heap.
 * @param double_free_handler Handler; the free is then ignored.
 */
void heap_set_double_free_handler(Heap *heap, DoubleFreeHandler double_free_handler);

/**
 * @brief Set the function called when corruption is detected, instead of asserting.
 *
 * @param heap Heap.
 * @param corruption_handler Handler.
 */
void heap_set_corruption_handler(Heap *heap, CorruptionHandler corruption_handler);

/**
 * @brief Allocate memory.
 *
 * @param heap Heap, initialized.
 * @param nbytes Number of bytes, greater than 0.
 * @param client_pc Caller address, recorded with @c CONFIG_MALLOC_INSTRUMENTATION.
 * @return Allocated memory, or NULL if there is not enough contiguous free space.
 */
void *heap_malloc(Heap *const heap, unsigned long nbytes, uintptr_t client_pc);

/**
 * @brief Free memory, merging it with adjacent free blocks.
 *
 * @param heap Heap, initialized.
 * @param ptr Memory allocated from @p heap, or NULL (no-op).
 * @param client_pc Caller address, for instrumentation.
 */
void heap_free(Heap *const heap, void *ptr, uintptr_t client_pc);

/**
 * @brief Reallocate memory.
 *
 * Always allocates a new block, copies as much of the old data as fits, then frees @p ptr.
 *
 * @param heap Heap.
 * @param ptr Memory to reallocate, or NULL.
 * @param nbytes New size in bytes.
 * @param client_pc Caller address, recorded with @c CONFIG_MALLOC_INSTRUMENTATION.
 * @return New memory, or NULL on failure (@p ptr is then left untouched).
 */
void *heap_realloc(Heap *const heap, void *ptr, unsigned long nbytes, uintptr_t client_pc);

/**
 * @brief Allocate zeroed memory.
 *
 * @param heap Heap.
 * @param size Number of bytes.
 * @param client_pc Caller address, for instrumentation.
 * @return Allocated memory, or NULL.
 */
void *heap_zalloc(Heap *const heap, size_t size, uintptr_t client_pc);

/**
 * @brief Allocate a zeroed array.
 *
 * The multiplication is not checked for overflow.
 *
 * @param heap Heap.
 * @param count Number of elements.
 * @param size Size of an element in bytes.
 * @param client_pc Caller address, for instrumentation.
 * @return Allocated memory, or NULL.
 */
void *heap_calloc(Heap *const heap, size_t count, size_t size, uintptr_t client_pc);

/**
 * @brief Check whether an address is within a heap.
 *
 * @param heap Heap.
 * @param ptr Address.
 * @return true if @p ptr is in the heap region.
 */
bool heap_contains_address(Heap *const heap, void *ptr);

/**
 * @brief Check whether a pointer is an allocated block of a heap.
 *
 * @param heap Heap.
 * @param ptr Pointer.
 * @return true if @p ptr was returned by an allocation and not freed.
 */
bool heap_is_allocated(Heap *const heap, void *ptr);

/**
 * @brief Get the size of a heap.
 *
 * @param heap Heap.
 * @return Size in bytes.
 */
size_t heap_size(const Heap *heap);

/**
 * @brief Get the smallest amount of free memory the heap has had.
 *
 * @param heap Heap.
 * @return Heap size minus the high water mark, in bytes.
 */
uint32_t heap_get_minimum_headroom(Heap *heap);

/**
 * @brief Calculate the current memory usage of a heap, for debugging.
 *
 * @param heap Heap.
 * @param[out] used Bytes allocated.
 * @param[out] free Bytes free.
 * @param[out] max_free Size of the largest free block.
 */
void heap_calc_totals(Heap *const heap, unsigned int *used, unsigned int *free,
                      unsigned int *max_free);

/**
 * @brief Dump every block and the heap totals to the debug serial port.
 *
 * Only available with @c CONFIG_MALLOC_INSTRUMENTATION.
 *
 * @param heap Heap.
 */
void heap_dump_malloc_instrumentation_to_dbgserial(Heap *heap);

/** @} */
