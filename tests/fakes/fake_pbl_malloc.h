/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "clar_asserts.h"
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
  int alloc_id;
} PointerListNode;

static PointerListNode *s_pointer_list = NULL;
static int s_alloc_id = 0;

static bool prv_pointer_list_filter(ListNode *node, void *ptr) {
  return ((PointerListNode *)node)->ptr == ptr;
}

static void prv_pointer_list_add(void *ptr, size_t bytes, void *lr) {
  PointerListNode *node = malloc(sizeof(PointerListNode));
  list_init(&node->list_node);
  node->ptr = ptr;
  node->bytes = bytes;
  node->lr = lr;
  node->alloc_id = ++s_alloc_id;
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
  size_t total_bytes = bytes * n;
  int alloc_id = s_alloc_id + 1;  // Will be assigned in prv_pointer_list_add
  if (total_bytes == 24) {
    printf("ALLOC 24 bytes: %p (id=%d, lr %p)\n", rt, alloc_id, lr);
  }
  prv_pointer_list_add(rt, total_bytes, lr);
  return rt;
}

void fake_malloc_set_largest_free_block(size_t bytes) {
  s_max_size_allowed = bytes;
}

static void free_and_track(void *ptr) {
  // Check if this might be a DiscoveryJobQueue by looking at the size
  printf("DEBUG: free_and_track looking for %p\n", ptr);
  ListNode *node = list_find((ListNode *)s_pointer_list, prv_pointer_list_filter, ptr);
  if (node) {
    PointerListNode *ptr_node = (PointerListNode *)node;
    printf("DEBUG: free_and_track found %p (id=%d) with size %zu\n", ptr, ptr_node->alloc_id, ptr_node->bytes);
    if (ptr_node->bytes == 24) {
      printf("FREE 24 bytes: %p (id=%d)\n", ptr, ptr_node->alloc_id);
    }
  } else {
    printf("DEBUG: free_and_track did NOT find %p in list\n", ptr);
  }
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
      printf("Still allocated: %p (id=%d, %zu bytes, lr %p)\n",
             ptr_node->ptr, ptr_node->alloc_id, ptr_node->bytes, ptr_node->lr);
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
  void *ptr = calloc_and_track(1, bytes, __builtin_return_address(0));
  if (bytes == 24) {
    printf("DEBUG: kernel_zalloc(24) = %p (caller %p)\n", ptr, __builtin_return_address(0));
  }
  return ptr;
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
