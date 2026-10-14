/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/util/heap.h>

#include <applib/app_heap_util.h>
#include <clar.h>
#include <fake_pebble_tasks.h>
#include <stubs_app_state.h>

#define BLOCK_SIZE sizeof(unsigned long)

// Stubs
///////////////////////////////////////////////////////////

void MPU_xTaskResumeAll(void) {
}

// Tests
///////////////////////////////////////////////////////////

void test_heap__should_handle_uniform_blocks(void) {
  // Smoke test:
  // - Alloc 15 uniform blocks
  // - Free 15 uniform blocks
  // - Alloc 15 uniform blocks
  const int heap_size_blocks = 30;
  int heap_size_bytes = BLOCK_SIZE * heap_size_blocks;
  void *heap_space = malloc(heap_size_bytes);
  cl_assert(heap_space != nullptr);

  Heap heap;

  // Test init
  heap_init(&heap, (void *)heap_space, (void *)(heap_space + heap_size_bytes), false);
  cl_assert(heap.begin == heap_space);
  cl_assert(heap.end == (heap_space + heap_size_bytes));
  cl_assert(heap.current_size == 0);

  void *ptr = nullptr;

  // Alloc
  for (int i = 0; i < heap_size_blocks / 2; i++) {
    // printf("Allocating block=<%d>\n", i);
    //  Test malloc
    ptr = heap_malloc(&heap, sizeof(BLOCK_SIZE), 0);
    cl_assert(ptr != nullptr);

    const int expected_remaining_blocks = heap_size_blocks - 2 * (i + 1);
    cl_assert(expected_remaining_blocks >= 0);

    if (i == 14) {
      // Heap should now be full
      cl_assert(heap.current_size == heap_size_bytes);
    } else {
      // Free block list should still be size 1
      cl_assert(heap.current_size == 2 * BLOCK_SIZE * (i + 1));
    }

    // Block is allocated at the *end* of the heap
    uint8_t *expected_addr = (uint8_t *)heap_space + i * BLOCK_SIZE * 2 + BLOCK_SIZE;
    printf("Expected addr at=<%p>; got ptr to=<%p>\n", expected_addr, (uint8_t *)ptr);
    cl_assert(expected_addr == ((uint8_t *)ptr));
  }

  ptr = heap_malloc(&heap, BLOCK_SIZE, 0);
  cl_assert(ptr == nullptr);
  cl_assert(heap.current_size == heap_size_blocks * BLOCK_SIZE);

  // Free
  // Reset ptr to first alloc'd element and iterate over all (uniform) elements
  ptr = ((uint8_t *)heap_space) + BLOCK_SIZE;
  for (int i = 0; i < heap_size_blocks / 2; i++) {
    heap_free(&heap, ptr, 0);
    cl_assert(heap.current_size == (heap_size_blocks - (2 * (i + 1))) * BLOCK_SIZE);
    ptr += 2 * BLOCK_SIZE;
  }

  cl_assert(heap.current_size == 0);

  // Alloc again to ensure free()'s worked
  for (int i = 0; i < heap_size_blocks / 2; i++) {
    // Test malloc
    ptr = heap_malloc(&heap, BLOCK_SIZE, 0);

    cl_assert(ptr != nullptr);
    const int expected_remaining_blocks = heap_size_blocks - 2 * (i + 1);
    cl_assert(expected_remaining_blocks >= 0);

    if (i == 14) {
      // Heap should now be full
      cl_assert(heap.current_size == heap_size_bytes);
    } else {
      // Free block list should still be size 1
      cl_assert(heap.current_size == 2 * BLOCK_SIZE * (i + 1));
    }
    // Block is allocated at the *end* of the heap
    uint8_t *expected_addr = (uint8_t *)heap_space + i * BLOCK_SIZE * 2 + BLOCK_SIZE;
    cl_assert(expected_addr == ((uint8_t *)ptr));
  }
  cl_assert(heap.begin == heap_space);

  ptr = heap_malloc(&heap, BLOCK_SIZE, 0);
  cl_assert(ptr == nullptr);
  cl_assert(heap.current_size == heap_size_blocks * BLOCK_SIZE);

  free(heap_space);
}

void test_heap__realloc(void) {
  int heap_size_bytes = BLOCK_SIZE * 15;
  void *heap_space = malloc(heap_size_bytes);
  cl_assert(heap_space != nullptr);

  Heap heap;

  heap_init(&heap, (void *)heap_space, (void *)(heap_space + heap_size_bytes), false);
  cl_assert(heap.begin == heap_space);

  unsigned int *ptr = nullptr;

  // Allocate a block, realloc to the same size, make sure data is the same.
  ptr = heap_malloc(&heap, sizeof(unsigned int) * 5, 0);
  for (int i = 0; i < 5; i++) {
    ptr[i] = i;
  }
  // Realloc, but OOM for requested size. Should return NULL...
  unsigned int *oom_ptr = heap_realloc(&heap, ptr, heap_size_bytes + 1, 0);
  cl_assert_equal_p(oom_ptr, nullptr);
  // ... but leave original block untouched:
  ptr = heap_realloc(&heap, ptr, sizeof(unsigned int) * 5, 0);
  for (int i = 0; i < 5; i++) {
    cl_assert(ptr[i] == i);
  }
  heap_free(&heap, ptr, 0);

  // Allocate a block, realloc to a larger size, make sure all data copied.
  ptr = heap_malloc(&heap, sizeof(unsigned int) * 5, 0);
  for (int i = 0; i < 5; i++) {
    ptr[i] = i;
  }
  ptr = heap_realloc(&heap, ptr, sizeof(unsigned int) * 10, 0);
  for (int i = 0; i < 5; i++) {
    cl_assert(ptr[i] == i);
  }
  heap_free(&heap, ptr, 0);

  // Allocate a block, realloc to a smaller size, make data copied.
  ptr = heap_malloc(&heap, sizeof(unsigned int) * 10, 0);
  for (int i = 0; i < 10; i++) {
    ptr[i] = i;
  }
  ptr = heap_realloc(&heap, ptr, sizeof(unsigned int) * 5, 0);
  for (int i = 0; i < 5; i++) {
    cl_assert(ptr[i] == i);
  }
  heap_free(&heap, ptr, 0);

  // realloc NULL:
  ptr = heap_realloc(&heap, nullptr, 10, 0);
  cl_assert(ptr);
  heap_free(&heap, ptr, 0);
}

void test_heap__should_handle_irregular_blocks(void) {
}

void test_heap__unaligned_start_end(void) {
  // Make a little word aligned buffer to use as our heap.
  uintptr_t int_buffer[8];
  char *char_buffer = (char *)&int_buffer[0];

  {
    Heap heap;
    heap_init(&heap, char_buffer + 1, char_buffer + 16, false);
    cl_assert_equal_i(heap_size(&heap), 8);
    cl_assert(heap_contains_address(&heap, char_buffer + 8));
    cl_assert(heap_contains_address(&heap, char_buffer + 12));
    cl_assert(!heap_contains_address(&heap, char_buffer + 2));
  }

  {
    Heap heap;
    heap_init(&heap, char_buffer + 8, char_buffer + 21, false);
    cl_assert_equal_i(heap_size(&heap), 8);
    cl_assert(heap_contains_address(&heap, char_buffer + 8));
    cl_assert(heap_contains_address(&heap, char_buffer + 12));
    cl_assert(!heap_contains_address(&heap, char_buffer + 18));
  }
}

void test_heap___heap_bytes_free(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);

  int heap_size_bytes = 1024;
  int malloc_size_bytes = 256;

  void *heap_space = malloc(heap_size_bytes);
  cl_assert(heap_space != nullptr);

  // Retrieve application heap, allocate space for it.
  Heap *heap = app_state_get_heap();
  heap_init(heap, (void *)heap_space, (void *)(heap_space + heap_size_bytes), false);
  cl_assert(heap->begin == heap_space);

  int before_available = heap_bytes_free();

  unsigned int *ptr = nullptr;
  ptr = heap_malloc(heap, malloc_size_bytes, 0);
  memset(ptr, 'X', malloc_size_bytes);

  int after_available = heap_bytes_free();

  // make sure the two values are within 16 bytes (usually shoule be 0-8, but 16 for safety)
  cl_assert(abs((before_available - malloc_size_bytes) - after_available) < 16);

  heap_free(heap, ptr, 0);
  free(heap_space);
}

void test_heap__heap_bytes_used(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);

  int heap_size_bytes = 1024;
  int malloc_size_bytes = 256;

  void *heap_space = malloc(heap_size_bytes);
  cl_assert(heap_space != nullptr);

  // Retrieve application heap, allocate space for it.
  Heap *heap = app_state_get_heap();
  heap_init(heap, (void *)heap_space, (void *)(heap_space + heap_size_bytes), false);
  cl_assert(heap->begin == heap_space);

  int before_used = heap_bytes_used();

  unsigned int *ptr = nullptr;
  ptr = heap_malloc(heap, malloc_size_bytes, 0);
  memset(ptr, 'X', malloc_size_bytes);

  int after_used = heap_bytes_used();

  // make sure the two values are within 16 bytes (usually shoule be 0-8, but 16 for safety)
  cl_assert(abs((before_used + malloc_size_bytes) - (after_used)) < 16);

  heap_free(heap, ptr, 0);
  free(heap_space);
}

void test_heap__is_allocated(void) {
  stub_pebble_tasks_set_current(PebbleTask_App);

  const size_t heap_size_bytes = 2048;

  void *heap_space = malloc(heap_size_bytes);
  cl_assert(heap_space != nullptr);

  // Retrieve application heap, allocate space for it.
  Heap *heap = app_state_get_heap();
  heap_init(heap, (void *)heap_space, (void *)(heap_space + heap_size_bytes), false);
  cl_assert(heap->begin == heap_space);

  // Allocate a few things
  const size_t malloc_size_bytes = 13;
  const size_t num_allocs = 10;
  void *allocs[num_allocs];
  for (size_t i = 0; i < num_allocs; ++i) {
    allocs[i] = heap_malloc(heap, malloc_size_bytes * i, 0);
  }

  void *needle = allocs[num_allocs / 2];
  cl_assert(heap_is_allocated(heap, needle));

  void *bad_needle = allocs[num_allocs / 3] + 1;
  cl_assert(!heap_is_allocated(heap, bad_needle));

  void *past_heap = (void *)heap->end + 1;
  cl_assert(!heap_is_allocated(heap, past_heap));

  void *pre_heap = (void *)heap->begin - 1;
  cl_assert(!heap_is_allocated(heap, pre_heap));

  void *freed_needle = allocs[num_allocs / 2];
  heap_free(heap, freed_needle, 0);
  cl_assert(!heap_is_allocated(heap, freed_needle));

  free(heap_space);
}

static void prv_alloc_and_test_fuzz_on_free(bool enabled) {
  stub_pebble_tasks_set_current(PebbleTask_App);

  const size_t heap_size_bytes = 2048;

  void *heap_space = malloc(heap_size_bytes);
  cl_assert(heap_space != nullptr);

  // Retrieve application heap, allocate space for it.
  Heap *heap = app_state_get_heap();
  heap_init(heap, (void *)heap_space, (void *)(heap_space + heap_size_bytes), enabled);
  cl_assert(heap->begin == heap_space);

  char *test_string = "data to store in heap";
  uint8_t *test = heap_malloc(heap, strlen(test_string), 0);
  memcpy(test, test_string, strlen(test_string));

  cl_assert_equal_i(memcmp(test, test_string, strlen(test_string)), 0);
  heap_free(heap, test, 0);

  if (enabled) {
    // the memory was fuzz'ed, better not match
    cl_assert(memcmp(test, test_string, strlen(test_string)) != 0);
  } else {
    // free'd data should match what was already there
    cl_assert_equal_i(memcmp(test, test_string, strlen(test_string)), 0);
  }

  free(heap_space);
}

void test_heap__fuzz_on_free(void) {
  prv_alloc_and_test_fuzz_on_free(true);
  prv_alloc_and_test_fuzz_on_free(false);
}

void test_heap__largest_free_block_after_smaller_hole(void) {
  const unsigned int heap_size_bytes = 128 * BLOCK_SIZE;
  uint8_t *heap_space = malloc(heap_size_bytes);
  cl_assert(heap_space != NULL);
  Heap heap;
  heap_init(&heap, heap_space, heap_space + heap_size_bytes, false);

  void *first = heap_malloc(&heap, 3 * BLOCK_SIZE, 0);
  void *guard = heap_malloc(&heap, BLOCK_SIZE, 0);
  void *larger = heap_malloc(&heap, 9 * BLOCK_SIZE, 0);
  void *second_guard = heap_malloc(&heap, BLOCK_SIZE, 0);
  void *tail = heap_malloc(&heap, 109 * BLOCK_SIZE, 0);
  cl_assert(first && guard && larger && second_guard && tail);

  heap_free(&heap, first, 0);
  heap_free(&heap, larger, 0);
  unsigned int used, free_bytes, max_free;
  heap_calc_totals(&heap, &used, &free_bytes, &max_free);
  cl_assert_equal_i(free_bytes, 14 * BLOCK_SIZE);
  cl_assert_equal_i(max_free, 10 * BLOCK_SIZE);

  heap_free(&heap, guard, 0);
  heap_free(&heap, second_guard, 0);
  heap_free(&heap, tail, 0);
  heap_calc_totals(&heap, &used, &free_bytes, &max_free);
  cl_assert_equal_i(used, 0);
  cl_assert_equal_i(free_bytes, heap_size_bytes);
  cl_assert_equal_i(max_free, heap_size_bytes);
  free(heap_space);
}
