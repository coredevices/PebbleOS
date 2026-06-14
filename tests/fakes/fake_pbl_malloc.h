/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <util/heap.h>
#include "util/list.h"
#include "util/math.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  ListNode list_node;
  size_t bytes;
  void *ptr;
  void *lr;
} PointerListNode;

static PointerListNode *s_pointer_list = NULL;

static bool prv_pointer_list_filter(ListNode *node, void *ptr) {
  return ((PointerListNode *)node)->ptr == ptr;
}

static void prv_pointer_list_add(void *ptr, size_t bytes, void *lr) {
  PointerListNode *node = malloc(sizeof(PointerListNode));
  list_init(&node->list_node);
  node->ptr = ptr;
  node->bytes = bytes;
  node->lr = lr;
  s_pointer_list = (PointerListNode *)list_prepend((ListNode *)s_pointer_list, &node->list_node);
}

static void prv_pointer_list_remove(void *ptr) {
  ListNode *node = list_find((ListNode *)s_pointer_list, prv_pointer_list_filter, ptr);
  if (!node && ptr) {
    printf("*** INVALID FREE: %p\n", ptr);
    cl_fail("Pointer has not been alloc'd (maybe a double free?)");
  }

  list_remove(node, (ListNode **)&s_pointer_list, NULL);
  free(node);
}

static size_t s_max_size_allowed = ~0;

static Heap s_heap;
Heap *task_heap_get_for_current_task(void) {
  return &s_heap;
}

static void *malloc_and_track(size_t bytes, void *lr) {
  if (bytes >= s_max_size_allowed)  {
    return NULL;
  }
  void *rt = malloc(bytes);
  prv_pointer_list_add(rt, bytes, lr);
  return rt;
}

static void *calloc_and_track(int n, size_t bytes, void *lr) {
  if ((bytes * n) >= s_max_size_allowed)  {
    return NULL;
  }

  void *rt = calloc(n, bytes);
  prv_pointer_list_add(rt, bytes, lr);
  return rt;
}

void fake_malloc_set_largest_free_block(size_t bytes) {
  s_max_size_allowed = bytes;
}

static void free_and_track(void *ptr) {
  prv_pointer_list_remove(ptr);
  free(ptr);
}

void *realloc_and_track(void *ptr, size_t bytes, void *lr) {
  void *new_ptr = malloc_and_track(bytes, lr);
  if (new_ptr && ptr) {
    ListNode *node = list_find((ListNode *)s_pointer_list, prv_pointer_list_filter, ptr);
    cl_assert(node);
    memcpy(new_ptr, ptr, MIN(((PointerListNode*)node)->bytes, bytes));
    free_and_track(ptr);
  }
  return new_ptr;
}

int fake_pbl_malloc_num_net_allocs(void) {
  return list_count((ListNode *)s_pointer_list);
}

void fake_pbl_malloc_check_net_allocs(void) {
  if (fake_pbl_malloc_num_net_allocs() > 0) {
    ListNode *node = (ListNode *)s_pointer_list;
    while (node) {
      PointerListNode *ptr_node = (PointerListNode *)node;
      printf("Still allocated: %p (%zu bytes, lr %p)\n",
             ptr_node->ptr, ptr_node->bytes, ptr_node->lr);
      node = list_get_next(node);
    }
  }
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void fake_pbl_malloc_clear_tracking(void) {
  while (s_pointer_list) {
    ListNode *new_head = list_pop_head((ListNode *)s_pointer_list);
    free(s_pointer_list);
    s_pointer_list = (PointerListNode *)new_head;
  }
  s_max_size_allowed = ~0;
}

void *task_malloc(size_t bytes) {
  return malloc_and_track(bytes, __builtin_return_address(0));
}

void *task_malloc_check(size_t bytes) {
  return malloc_and_track(bytes, __builtin_return_address(0));
}

void *task_realloc(void *ptr, size_t bytes) {
  return realloc_and_track(ptr, bytes, __builtin_return_address(0));
}

void *task_zalloc(size_t bytes) {
  void *ptr = task_malloc(bytes);
  if (ptr) {
    memset(ptr, 0, bytes);
  }
  return ptr;
}

void *task_zalloc_check(size_t bytes) {
  void *ptr = task_malloc_check(bytes);
  memset(ptr, 0, bytes);
  return ptr;
}

void *task_calloc(size_t count, size_t size) {
  return calloc_and_track(count, size, __builtin_return_address(0));
}

void *task_calloc_check(size_t count, size_t size) {
  return calloc_and_track(count, size, __builtin_return_address(0));
}

void task_free(void *ptr) {
  free_and_track(ptr);
}

void *applib_zalloc(size_t bytes) {
  return calloc_and_track(1, bytes, __builtin_return_address(0));
}

void applib_free(void *ptr) {
  free_and_track(ptr);
}

void *app_malloc(size_t bytes) {
  return malloc_and_track(bytes, __builtin_return_address(0));
}

void *app_malloc_check(size_t bytes) {
  return malloc_and_track(bytes, __builtin_return_address(0));
}

void app_free(void *ptr) {
  free_and_track(ptr);
}

void *kernel_malloc(size_t bytes) {
  return malloc_and_track(bytes, __builtin_return_address(0));
}

void *kernel_zalloc(size_t bytes) {
  return calloc_and_track(1, bytes, __builtin_return_address(0));
}

void *kernel_zalloc_check(size_t bytes) {
  return kernel_zalloc(bytes);
}

void *kernel_malloc_check(size_t bytes) {
  return malloc_and_track(bytes, __builtin_return_address(0));
}

void *kernel_realloc(void *ptr, size_t bytes) {
  return realloc_and_track(ptr, bytes, __builtin_return_address(0));
}

void kernel_free(void *ptr) {
  free_and_track(ptr);
}

void* kernel_calloc(size_t count, size_t size) {
  return calloc_and_track(count, size, __builtin_return_address(0));
}

char* kernel_strdup(const char* s) {
  char *r = malloc_and_track(strlen(s) + 1, __builtin_return_address(0));
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
  free_and_track(ptr);
}
