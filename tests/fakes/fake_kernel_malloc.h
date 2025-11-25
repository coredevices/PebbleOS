/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <string.h>

#include "util/list.h"

#include "clar_asserts.h"

// Simple pass-through implementation of kernel_malloc/free that attempts to
// protect against buffer overruns in tested code by adding a canary value to
// the beginning and end of the allocated block, and verifying the value on
// freeing of the block. It won't catch *all* memory errors of course, like
// writing way outside of your bounds, or use-after-free, or neglecting to free.
// But it should catch some of the simpler cases.

static const uint32_t s_malloc_canary = 0x54761F34;

static uint64_t s_largest_free_block_bytes = ~0;

static uint64_t s_heap_mark;

static bool s_stats_enabled = false;

typedef struct {
  ListNode node;
  size_t size;
  void *ptr;
} Allocation;

Allocation *s_head;

void* kernel_malloc(size_t bytes) {
  if (bytes > s_largest_free_block_bytes) {
    return NULL;
  }

  char* memory = malloc(bytes + 12);

  memcpy(memory, &bytes, 4);
  memcpy(memory + 4, &s_malloc_canary, 4);
  memcpy(memory + bytes + 8, &s_malloc_canary, 4);

  void* ptr = memory + 8;

  if (s_stats_enabled) {
    Allocation *a = (Allocation *) malloc(sizeof(Allocation));
    *a = (const Allocation) {
      .size = bytes,
      .ptr = ptr,
    };
    s_head = (Allocation *) list_prepend((ListNode *) s_head, &a->node);
  }

  return ptr;
}

void* kernel_zalloc(size_t bytes) {
  void *ptr = kernel_malloc(bytes);
  if (ptr) {
    memset(ptr, 0, bytes);
  }
  return ptr;
}

void* kernel_zalloc_check(size_t bytes) {
  return kernel_zalloc(bytes);
}

void* kernel_malloc_check(size_t bytes) {
  return kernel_malloc(bytes);
}

char* kernel_strdup(const char* s) {
  char *r = kernel_malloc_check(strlen(s) + 1);
  if (!r) {
    return NULL;
  }
  strcpy(r, s);
  return r;
}

char* kernel_strdup_check(const char* s) {
  return kernel_strdup(s);
}

static bool prv_find_allocation_filter_cb(ListNode *found_node, void *data) {
  Allocation *a = (Allocation *) found_node;
  return (a->ptr == data);
}

// Split into its own function to make it easy to set a breakpoint on it when debugging
// using `./waf test --debug_test`
static void prv_double_free_assert(Allocation *a) {
  cl_assert_(a != NULL, "Couldn't find allocation! Double free?");
}

void kernel_free(void* ptr) {
  if (ptr == NULL) {
    return;
  }

  if (s_stats_enabled) {
    Allocation *a = (Allocation *) list_find((ListNode *) s_head,
                                             prv_find_allocation_filter_cb, ptr);
    prv_double_free_assert(a);
    list_remove(&a->node, (ListNode **) &s_head, NULL);
    free(a);
  }

  char* memory = (char*)ptr - 8;

  uint32_t canary_start = -1;
  uint32_t canary_end = -1;
  uint32_t canary_length = -1;

  memcpy(&canary_length, memory, 4);
  memcpy(&canary_start, memory + 4, 4);
  memcpy(&canary_end, memory + canary_length + 8, 4);

  cl_assert(canary_start == s_malloc_canary);
  cl_assert(canary_length != -1);
  cl_assert(canary_end == s_malloc_canary);

  free(memory);
}

//! Enables or disables the tracking of allocations
void fake_kernel_malloc_enable_stats(bool enable) {
  s_stats_enabled = enable;
}

//! Returns the number of bytes allocated on the kernel heap.
//! @note Call fake_kernel_malloc_enable_stats(true) before using this.
uint64_t fake_kernel_malloc_get_total_bytes_allocated(void) {
  uint64_t bytes_allocated = 0;
  Allocation *a = s_head;
  while (a) {
    bytes_allocated += a->size;
    a = (Allocation *) a->node.next;
  }
  return bytes_allocated;
}

//! Makes successive kernel_malloc() fail for sizes above the number of bytes specified.
void fake_kernel_malloc_set_largest_free_block(uint64_t bytes) {
  s_largest_free_block_bytes = bytes;
}

//! Marks the current, total bytes allocated.
//! @see fake_kernel_malloc_mark_assert_equal
void fake_kernel_malloc_mark(void) {
  s_heap_mark = fake_kernel_malloc_get_total_bytes_allocated();
}

//! Asserts that the total bytes allocated is the same as the last time fake_kernel_malloc_mark()
//! was called.
void fake_kernel_malloc_mark_assert_equal(void) {
  cl_assert_equal_i(s_heap_mark, fake_kernel_malloc_get_total_bytes_allocated());
}

void fake_kernel_malloc_init(void) {
  s_largest_free_block_bytes = ~0;
  s_heap_mark = 0;
  s_head = NULL;
}

void fake_kernel_malloc_deinit(void) {
  Allocation *a = s_head;
  while (a) {
    Allocation *next = (Allocation *) a->node.next;
    free(a);
    a = next;
  }
  s_head = NULL;
}
