/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/heap.h>
#include <pbl/logging/logging.h>
#include <pbl/util/logging.h>
#include <pbl/util/math.h>

// With AddressSanitizer, only what is allocated is addressable. This file is
// not instrumented itself, as it walks block headers and free memory.
#ifdef CONFIG_ASAN
#include <sanitizer/asan_interface.h>
#define HEAP_POISON(addr, size)   ASAN_POISON_MEMORY_REGION(addr, size)
#define HEAP_UNPOISON(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)
#else
#define HEAP_POISON(addr, size)
#define HEAP_UNPOISON(addr, size)
#endif

/* The following defines a type that is the size in bytes of the     */
/* desired alignment of each data fragment.                          */
typedef unsigned long Alignment_t;

/* The following defines the byte boundary size that has been        */
/* specified size if the alignment data.                             */
#define ALIGNMENT_SIZE sizeof(Alignment_t)

/* The following structure is used to align data fragments on a      */
/* specified memory boundary.                                        */
typedef union _tagAlignmentStruct_t {
  Alignment_t AlignmentValue;
  unsigned char ByteValue;
} AlignmentStruct_t;

/* The following defines the size in bytes of a data fragment that is*/
/* considered a large value.  Allocations that are equal to and      */
/* larger than this value will be allocated from the end of the      */
/* buffer.                                                           */
#define LARGE_SIZE (256 / ALIGNMENT_SIZE)

//! The maximum size of the heap as a number of ALIGNMENT_SIZE
#define SEGMENT_SIZE_MAX (0x7FFF)

/* The following defines the minimum size (in alignment units) of a  */
/* fragment that is considered useful.  The value is used when trying*/
/* to determine if a fragment that is larger than the requested size */
/* can be broken into 2 fragments leaving a fragment that is of the  */
/* requested size and one that is at least as larger as the          */
/* MINIMUM_MEMORY_SIZE.                                              */
#define MINIMUM_MEMORY_SIZE 1

/* The following defines the structure that describes a memory       */
/* fragment.                                                         */
typedef struct _tagHeapInfo_t {
  //! Size of the preceding segment, measured in units of ALIGNMENT_SIZE, including the size of this
  //! beginner
  uint16_t PrevSize;

  //! Whether or not this block is currently allocated (vs being free).
  bool is_allocated : 1;

  //! Size of this segment, measured in units of ALIGNMENT_SIZE, including this size of this
  //! beginner
  uint16_t Size : 15;

#ifdef CONFIG_MALLOC_INSTRUMENTATION
  uintptr_t pc; //<! The address that called malloc.
#endif

  //! This is the actual buffer that's returned to the caller. We use this struct to make
  //! sure the buffer we return is appropriately aligned.
  AlignmentStruct_t Data;
} HeapInfo_t;

//! The size of a block in units of Alignment_t, including the beginner and including _x words of
//! data.
#define HEAP_INFO_BLOCK_SIZE(_x) ((offsetof(HeapInfo_t, Data) / ALIGNMENT_SIZE) + (_x))

//! Convert a pointer to the Data member to a pointer to the HeapInfo_t beginner
#define HEAP_INFO_FOR_PTR(ptr) (HeapInfo_t *)(((Alignment_t *)ptr) - HEAP_INFO_BLOCK_SIZE(0))

static_assert((offsetof(HeapInfo_t, Data) % ALIGNMENT_SIZE) == 0, "Heap not properly aligned.");

//! Heap is assumed corrupt if expr does not evaluate true
#define HEAP_ASSERT_SANE(heap, expr, log_addr) \
  if (!(expr)) {                               \
    prv_handle_corruption(heap, log_addr);     \
  }

static void prv_lock(struct pbl_heap *heap) {
  if (heap->lock.lock) {
    heap->lock.lock(heap->lock.ctx);
  }
}

static void prv_unlock(struct pbl_heap *heap) {
  if (heap->lock.unlock) {
    heap->lock.unlock(heap->lock.ctx);
  }

  // Handle any heap corruption that may have been detected while the heap was locked.
  if (heap->corrupt_block != nullptr) {
    heap->corruption_handler(heap->corrupt_block);
    heap->corrupt_block = nullptr;
  }
}

static void prv_handle_corruption(struct pbl_heap *heap, void *ptr) {
  if (heap->corruption_handler) {
    heap->corrupt_block = ptr;
    return;
  }
  PBL_ASSERTN(0); // Error: Heap corrupt around <ptr>
}

static HeapInfo_t *find_segment(struct pbl_heap *heap, unsigned long n_units);
static HeapInfo_t *allocate_block(struct pbl_heap *heap, unsigned long n_units,
                                  HeapInfo_t *heap_info_ptr);

//! Advance the block pointer to the next block.
static HeapInfo_t *get_next_block(struct pbl_heap *heap, HeapInfo_t *block) {
  HEAP_ASSERT_SANE(heap, block->Size != 0, block);
  return (HeapInfo_t *)(((Alignment_t *)block) + block->Size);
}

//! Move the block pointer back to the previous block.
static HeapInfo_t *get_previous_block(struct pbl_heap *heap, HeapInfo_t *block) {
  HEAP_ASSERT_SANE(heap, block->PrevSize != 0, block);
  return (HeapInfo_t *)(((Alignment_t *)block) - block->PrevSize);
}

static void prv_calc_totals(struct pbl_heap *heap, unsigned int *used, unsigned int *free,
                            unsigned int *max_free) {
  HeapInfo_t *heap_info_ptr;

  /* Initialize the return values.                                  */
  *used = 0;
  *free = 0;
  *max_free = 0;
  heap_info_ptr = heap->begin;

  do {
    /* Check to see if the current fragment is marked as free.     */
    if (heap_info_ptr->is_allocated) {
      *used += heap_info_ptr->Size * ALIGNMENT_SIZE;
    } else {
      /* Accumulate the total number of free Bytes.               */
      *free += heap_info_ptr->Size * ALIGNMENT_SIZE;

      /* Check to see if the current fragment is larger that any  */
      /* we have seen and update the Max Value if it is larger.   */
      if (heap_info_ptr->Size * ALIGNMENT_SIZE > *max_free) {
        *max_free = heap_info_ptr->Size * ALIGNMENT_SIZE;
      }
    }

    /* Adjust the pointer to the next entry.                       */
    heap_info_ptr = get_next_block(heap, heap_info_ptr);

  } while (heap_info_ptr < heap->end);

  PBL_ASSERTN(heap_info_ptr == heap->end);
}

void pbl_heap_stats(struct pbl_heap *heap, unsigned int *used, unsigned int *free,
                    unsigned int *max_free) {
  PBL_ASSERTN(heap);

  /* Verify that the parameters that were passed in appear valid.      */
  if ((used) && (free) && (max_free) && (heap->begin)) {
    prv_lock(heap);
    prv_calc_totals(heap, used, free, max_free);
    prv_unlock(heap);
  }
}

void pbl_heap_init(struct pbl_heap *heap, void *start, void *end, bool fuzz_on_free) {
  PBL_ASSERTN(start && end);

  // Align the pointer by advancing it to the next boundary.
  start = (void *)((((uintptr_t)start) + (sizeof(Alignment_t) - 1)) & ~(sizeof(Alignment_t) - 1));
  end = (void *)(((uintptr_t)end) & ~(sizeof(Alignment_t) - 1));

  // Calculate the size of the heap in alignment units.
  uint32_t heap_size = ((uintptr_t)end - (uintptr_t)start) / ALIGNMENT_SIZE;
  // If we have more space than we can use, just limit it to the usable space. This limit is caused
  // by the width of .Size and .PrevSize in HeapInfo_t
  heap_size = MIN(SEGMENT_SIZE_MAX, heap_size);
  end = ((char *)start) + (heap_size * ALIGNMENT_SIZE);

  memset(start, 0, heap_size * ALIGNMENT_SIZE);

  *heap = (struct pbl_heap){.begin = start, .end = end, .fuzz_on_free = fuzz_on_free};

  *(heap->begin) = (HeapInfo_t){.PrevSize = heap_size, .is_allocated = false, .Size = heap_size};
  HEAP_POISON(start, heap_size * ALIGNMENT_SIZE);
}

void pbl_heap_set_lock(struct pbl_heap *heap, struct pbl_heap_lock lock) {
  heap->lock = lock;
}

void pbl_heap_set_double_free_handler(struct pbl_heap *heap, pbl_heap_error_handler_t handler) {
  heap->double_free_handler = handler;
}

void pbl_heap_set_corruption_handler(struct pbl_heap *heap, pbl_heap_error_handler_t handler) {
  heap->corruption_handler = handler;
}

void *pbl_heap_alloc(struct pbl_heap *heap, size_t nbytes, uintptr_t caller) {
  // Check to make sure the heap we have is initialized.
  PBL_ASSERTN(heap->begin);

  /* Convert the requested memory allocation in bytes to alignment     */
  /* size.                                                             */
  unsigned long allocation_size = (nbytes + (ALIGNMENT_SIZE - 1)) / ALIGNMENT_SIZE;

  /* Add the beginner size to the requested size.                        */
  allocation_size += HEAP_INFO_BLOCK_SIZE(0);

  /* Verify that the requested size is valid                           */
  if ((allocation_size < HEAP_INFO_BLOCK_SIZE(1)) || (allocation_size >= SEGMENT_SIZE_MAX)) {
    return nullptr;
  }

  HeapInfo_t *allocated_block;

  prv_lock(heap);
  {
    HeapInfo_t *free_block = find_segment(heap, allocation_size);
    allocated_block = allocate_block(heap, allocation_size, free_block);

    if (allocated_block != nullptr) {
      // We've allocated a new block, update our metrics

#ifdef CONFIG_MALLOC_INSTRUMENTATION
      allocated_block->pc = caller;
#endif

      heap->current_size += allocated_block->Size * ALIGNMENT_SIZE;
      if (heap->current_size > heap->high_water_mark) {
        heap->high_water_mark = heap->current_size;
      }
    }
  }
  prv_unlock(heap);

  if (allocated_block) {
    HEAP_UNPOISON(&allocated_block->Data,
                  (allocated_block->Size - HEAP_INFO_BLOCK_SIZE(0)) * ALIGNMENT_SIZE);
    return &allocated_block->Data;
  }
  return nullptr;
}

void pbl_heap_free(struct pbl_heap *heap, void *ptr, uintptr_t caller) {
  PBL_ASSERTN(heap->begin);

  if (!ptr) {
    // free(0) is a no-op, just bail
    return;
  }

  PBL_ASSERTN(pbl_heap_contains(heap, ptr)); // <ptr> not in range (heap->begin, heap->end)

  prv_lock(heap);
  {
    /* Get a pointer to the Heap Info.                             */
    HeapInfo_t *heap_info_ptr = HEAP_INFO_FOR_PTR(ptr);

    /* Verify that this segment is allocated.                      */
    if (!heap_info_ptr->is_allocated) {
      // If not, try to the call the handler if present. If there's no handler, just explode.
      // If there is a handler, let them know this happened and then just no-op and fast return.
      if (heap->double_free_handler) {
        prv_unlock(heap);

        heap->double_free_handler(ptr);

        return;
      }

      PBL_ASSERTN(0); // heap_free on invalid pointer <ptr>
    }

    /* Mask out the allocation bit of the segment to be freed.  */
    /* This will make calculations in this block easier.        */
    heap_info_ptr->is_allocated = false;

#ifndef CONFIG_RELEASE
    if (heap->fuzz_on_free) {
      memset(ptr, 0xBD, (heap_info_ptr->Size - HEAP_INFO_BLOCK_SIZE(0)) * ALIGNMENT_SIZE);
    }
#endif
    HEAP_POISON(ptr, (heap_info_ptr->Size - HEAP_INFO_BLOCK_SIZE(0)) * ALIGNMENT_SIZE);

    // Update metrics
#ifdef CONFIG_MALLOC_INSTRUMENTATION
    heap_info_ptr->pc = caller;
#endif
    heap->current_size -= heap_info_ptr->Size * ALIGNMENT_SIZE;

    /* If the segment to be freed is at the begin of the heap,   */
    /* then we do not have to merge or update any sizes of the  */
    /* previous segment.  This will also handle the case where  */
    /* the entire heap has been allocated to one segment.       */
    if (heap_info_ptr != heap->begin) {
      /* Calculate the pointer to the previous segment.        */
      HeapInfo_t *previous_block = get_previous_block(heap, heap_info_ptr);

      HEAP_ASSERT_SANE(heap, previous_block->Size == heap_info_ptr->PrevSize, heap_info_ptr);

      /* Check to see if the previous segment can be combined. */
      if (!previous_block->is_allocated) {
        /* Add the segment to be freed to the new beginner.     */
        previous_block->Size += heap_info_ptr->Size;

        /* Set the pointer to the beginning of the previous   */
        /* free segment.                                      */
        heap_info_ptr = previous_block;
      }
    }

    /* Calculate the pointer to the next segment.               */
    HeapInfo_t *next_block = get_next_block(heap, heap_info_ptr);

    /* If we are pointing at the end of the heap, then use the  */
    /* begin as the next segment.                                */
    if (next_block == heap->end) {
      /* We can't combine the begin with the end, so just      */
      /* update the PrevSize field.                            */
      heap->begin->PrevSize = heap_info_ptr->Size;
    } else {
      HEAP_ASSERT_SANE(heap, next_block->PrevSize == (HEAP_INFO_FOR_PTR(ptr))->Size, next_block);

      /* We are not pointing to the end of the heap, so if the */
      /* next segment is allocated then update the PrevSize    */
      /* field.                                                */
      if (next_block->is_allocated) {
        next_block->PrevSize = heap_info_ptr->Size;
      } else {
        /* The next segment is free, so merge it with the     */
        /* current segment.                                   */
        heap_info_ptr->Size += next_block->Size;

        /* Since we merged the next segment, we have to update*/
        /* the next next segment's PrevSize field.            */
        HeapInfo_t *next_next_block = get_next_block(heap, heap_info_ptr);

        /* If we are pointing at the end of the heap, then use*/
        /* the begin as the next next segment.                 */
        if (next_next_block == heap->end) {
          heap->begin->PrevSize = heap_info_ptr->Size;
        } else {
          next_next_block->PrevSize = heap_info_ptr->Size;
        }
      }
    }
  }
  prv_unlock(heap);
}

bool pbl_heap_is_allocated(struct pbl_heap *heap, void *ptr) {
  bool rc = false;
  if (!pbl_heap_contains(heap, ptr)) {
    return rc;
  }

  HeapInfo_t *heap_info_ptr = heap->begin;

  // Iterate through the heap to see if this pointer is still allocated.
  prv_lock(heap);
  while (heap_info_ptr < heap->end) {
    if (heap_info_ptr == HEAP_INFO_FOR_PTR(ptr)) {
      rc = heap_info_ptr->is_allocated;
      break;
    }
    heap_info_ptr = get_next_block(heap, heap_info_ptr);
  }
  prv_unlock(heap);

  return rc;
}

bool pbl_heap_contains(const struct pbl_heap *heap, const void *ptr) {
  return (ptr >= (const void *)heap->begin && ptr < (const void *)heap->end);
}

size_t pbl_heap_size(const struct pbl_heap *heap) {
  return ((char *)heap->end) - ((char *)heap->begin);
}

static void prv_sanity_check_block(struct pbl_heap *heap, HeapInfo_t *block) {
  HeapInfo_t *prev_block = get_previous_block(heap, block);
  HEAP_ASSERT_SANE(heap, prev_block <= heap->begin || prev_block->Size == block->PrevSize, block);
  HeapInfo_t *next_block = get_next_block(heap, block);
  HEAP_ASSERT_SANE(heap, next_block >= heap->end || next_block->PrevSize == block->Size, block);
}

//! Finds a segment where data of the size n_units  will fit.
//!     @param heap the heap to search.
//!     @param n_units number of ALIGNMENT_SIZE units this segment requires.
static HeapInfo_t *find_segment(struct pbl_heap *heap, unsigned long n_units) {
  HeapInfo_t *heap_info_ptr = nullptr;
  /* If we are allocating a large segment, then start at the  */
  /* end of the heap.  Otherwise, start at the beginning of   */
  /* the heap.  If there is only one segment in the heap, then*/
  /* heap->begin will be used either way.                      */
  if (n_units >= LARGE_SIZE) {
    heap_info_ptr = (HeapInfo_t *)(((Alignment_t *)heap->end) - heap->begin->PrevSize);
  } else {
    heap_info_ptr = heap->begin;
  }

  /* Loop until we have walked the entire list.               */
  while (((n_units < LARGE_SIZE) || (heap_info_ptr > heap->begin)) && (heap_info_ptr < heap->end)) {
    prv_sanity_check_block(heap, heap_info_ptr);

    /* Check to see if the current entry is free and is large*/
    /* enough to hold the data being requested.              */
    if (heap_info_ptr->is_allocated || (heap_info_ptr->Size < n_units)) {
      /* If the requested size is larger than the limit then*/
      /* search backwards for an available buffer, else go  */
      /* forward.  This will hopefully help to reduce       */
      /* fragmentation problems.                            */
      if (n_units >= LARGE_SIZE) {
        heap_info_ptr = get_previous_block(heap, heap_info_ptr);
      } else {
        heap_info_ptr = get_next_block(heap, heap_info_ptr);
      }
    } else {
      break;
    }
  }

  // make sure the space we found is within the bounds of the heap
  PBL_ASSERTN((heap_info_ptr >= heap->begin) && (heap_info_ptr <= heap->end));

  return heap_info_ptr;
}

//! Split a block into two smaller blocks, returning a pointer to the new second block.
//! The first block will be available at the same location as before, but with a smaller size.
//! Assumes the block is big enough to be split and is unallocated.
//! @param heap the heap the block belongs to.
//! @param block the block to split.
//! @param first_part_size the size of the new block, in ALIGNMENT_SIZE units, including the
//! beginner
static HeapInfo_t *split_block(struct pbl_heap *heap, HeapInfo_t *block, size_t first_part_size) {
  HeapInfo_t *second_block = (HeapInfo_t *)(((Alignment_t *)block) + first_part_size);

  second_block->PrevSize = first_part_size;
  second_block->is_allocated = false;
  second_block->Size = block->Size - first_part_size;

  block->Size = first_part_size;

  /* Calculate the pointer to the next segment.         */
  HeapInfo_t *next_next_block = get_next_block(heap, second_block);

  /* Check for a wrap condition and update the next     */
  /* segment's PrevSize field.                          */
  if (next_next_block == heap->end) {
    heap->begin->PrevSize = second_block->Size;
  } else {
    next_next_block->PrevSize = second_block->Size;
  }

  return second_block;
}

//! Allocated the block in the given HeapInfo_t segment.
//!     @param heap the heap to allocate from.
//!     @param n_units number of ALIGNMENT_SIZE units this segment requires (including space for the
//!     beginner).
//!     @param heap_info_ptr the segment where the block should be allocated.
static HeapInfo_t *allocate_block(struct pbl_heap *heap, unsigned long n_units,
                                  HeapInfo_t *heap_info_ptr) {
  // Make sure we can use all or part of this block for this allocation.
  if (heap_info_ptr == heap->end || heap_info_ptr->is_allocated || heap_info_ptr->Size < n_units) {
    return nullptr;
  }

  /* Check to see if we need to split this into two        */
  /* entries.                                              */
  /* * NOTE * If there is not enough room to make another  */
  /*          entry then we will not adjust the size of    */
  /*          this entry to match the amount requested.    */
  if (heap_info_ptr->Size < (n_units + HEAP_INFO_BLOCK_SIZE(MINIMUM_MEMORY_SIZE))) {
    // Nope! Just use the whole block.
    heap_info_ptr->is_allocated = true;
    return heap_info_ptr;
  }

  /* If this is a large segment allocation, then split  */
  /* the segment so that the free segment is at the     */
  /* beginning.                                         */
  if (n_units >= LARGE_SIZE) {
    HeapInfo_t *second_block = split_block(heap, heap_info_ptr, heap_info_ptr->Size - n_units);
    second_block->is_allocated = true;
    return second_block;
  }

  split_block(heap, heap_info_ptr, n_units);
  heap_info_ptr->is_allocated = true;
  return heap_info_ptr;
}

// A very naive realloc implementation
void *pbl_heap_realloc(struct pbl_heap *heap, void *ptr, size_t nbytes, uintptr_t caller) {
#if !defined(CONFIG_MALLOC_INSTRUMENTATION)
  caller = 0;
#endif
  // Get a pointer to the Heap Info.
  void *new_ptr = pbl_heap_alloc(heap, nbytes, caller);
  if (new_ptr && ptr) {
    // Copy over old data.
    const HeapInfo_t *heap_info_ptr = HEAP_INFO_FOR_PTR(ptr);
    const size_t original_size = (heap_info_ptr->Size - HEAP_INFO_BLOCK_SIZE(0)) * ALIGNMENT_SIZE;
    memcpy(new_ptr, ptr, MIN(nbytes, original_size));
    pbl_heap_free(heap, ptr, caller);
  }
  return new_ptr;
}

void *pbl_heap_zalloc(struct pbl_heap *heap, size_t size, uintptr_t caller) {
  void *ptr = pbl_heap_alloc(heap, size, caller);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

void *pbl_heap_calloc(struct pbl_heap *heap, size_t count, size_t size, uintptr_t caller) {
  size_t total;

  if (ckd_mul(&total, count, size)) {
    return nullptr;
  }
  return pbl_heap_zalloc(heap, total, caller);
}

char *pbl_heap_strdup(struct pbl_heap *heap, const char *s, uintptr_t caller) {
  const size_t size = strlen(s) + 1;
  char *dup = pbl_heap_alloc(heap, size, caller);
  if (dup) {
    memcpy(dup, s, size);
  }
  return dup;
}

[[noreturn]] void pbl_heap_oom(struct pbl_heap *heap, size_t size, uintptr_t caller) {
  unsigned int used = 0, free_bytes = 0, max_free = 0;
  pbl_heap_stats(heap, &used, &free_bytes, &max_free);
  PBL_LOG_ALWAYS("OOM: Failed to alloc %zu bytes at LR: 0x%" PRIxPTR
                 " (used %u, free %u, max_free %u)",
                 size, caller, used, free_bytes, max_free);

#ifdef CONFIG_MALLOC_INSTRUMENTATION
  pbl_heap_dump(heap);
#endif

  pbl_heap_oom_handler(heap, size, caller);
}

size_t pbl_heap_used(const struct pbl_heap *heap) {
  return heap->current_size;
}

size_t pbl_heap_min_headroom(const struct pbl_heap *heap) {
  return pbl_heap_size(heap) - heap->high_water_mark;
}

void pbl_heap_reset_high_water_mark(struct pbl_heap *heap) {
  prv_lock(heap);
  heap->high_water_mark = heap->current_size;
  prv_unlock(heap);
}

// Serial Commands
///////////////////////////////////////////////////////////
#ifdef CONFIG_MALLOC_INSTRUMENTATION
void pbl_heap_dump(struct pbl_heap *heap, pbl_heap_dump_cb_t cb, void *arg) {
  char buffer[80];
  unsigned long num_free_blocks = 0;
  unsigned long num_free_bytes = 0;
  unsigned long num_alloc_blocks = 0;
  unsigned long num_alloc_bytes = 0;
  unsigned long largest_free = 0;

  // The output in this function is parsed by tools/parse_dump_malloc.py, so don't change it
  // without updating that file as well.

  HeapInfo_t *heap_iter = heap->begin;

  prv_lock(heap);
  void *pc;
  char *type;
  while (heap_iter < heap->end) {
    unsigned long block_size = (long)(heap_iter->Size) * ALIGNMENT_SIZE;

    if (heap_iter->is_allocated) {
      pc = (void *)heap_iter->pc;
      type = "";
      num_alloc_blocks++;
      num_alloc_bytes += block_size;
    } else {
      pc = nullptr;
      type = "FREE";
      num_free_blocks++;
      num_free_bytes += block_size;
      largest_free = MAX(largest_free, block_size);
    }

    snprintf(buffer, sizeof(buffer), "PC:0x%08lX Addr:0x%08lX Bytes:%-8lu %s", (long)pc,
             (long)&heap_iter->Data, block_size, type);
    cb(buffer, arg);
    heap_iter = get_next_block(heap, heap_iter);
  }

  snprintf(buffer, sizeof(buffer), "Heap start %p", heap->begin);
  cb(buffer, arg);

  snprintf(buffer, sizeof(buffer), "Heap end %p", heap->end);
  cb(buffer, arg);

  snprintf(buffer, sizeof(buffer), "Heap total size %zu", pbl_heap_size(heap));
  cb(buffer, arg);

  snprintf(buffer, sizeof(buffer), "Heap allocated %u", heap->current_size);
  cb(buffer, arg);

  snprintf(buffer, sizeof(buffer), "Heap high water mark %u", heap->high_water_mark);
  cb(buffer, arg);

  snprintf(buffer, sizeof(buffer), "Heap free blocks: %lu bytes, %lu blocks", num_free_bytes,
           num_free_blocks);
  cb(buffer, arg);

  snprintf(buffer, sizeof(buffer), "Heap alloc blocks: %lu bytes, %lu blocks", num_alloc_bytes,
           num_alloc_blocks);
  cb(buffer, arg);

  snprintf(buffer, sizeof(buffer), "Heap largest free block: %lu", largest_free);
  cb(buffer, arg);

  prv_unlock(heap);
}
#endif
