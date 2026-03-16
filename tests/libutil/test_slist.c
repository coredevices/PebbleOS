/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/slist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clar.h"
#include "stubs_passert.h"

// Stubs
///////////////////////////////////////////////////////////
int g_pbl_log_level = 0;
void pbl_log(int level, const char* src_filename, int src_line_number, const char* fmt, ...) { }

// Tests
///////////////////////////////////////////////////////////

void test_slist__initialize(void) {
}

void test_slist__cleanup(void) {
}

void test_slist__insert_after(void) {
  SingleListNode *tail = NULL;
  SingleListNode a = SINGLE_LIST_NODE_NULL, b = SINGLE_LIST_NODE_NULL;
  tail = slist_insert_after(tail, &a);
  cl_assert(tail == &a);
  tail = slist_insert_after(&a, &b);
  cl_assert(tail == &b);
  cl_assert(a.next == &b);
  cl_assert(b.next == NULL);
}

void test_slist__insert_after_middle(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  a.next = &c;
  slist_insert_after(&a, &b);
  cl_assert(a.next == &b);
  cl_assert(b.next == &c);
  cl_assert(c.next == NULL);
}

void test_slist__prepend(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  SingleListNode *head;
  head = slist_prepend(&c, &b);
  cl_assert(head == &b);
  cl_assert(b.next == &c);
  head = slist_prepend(&b, &a);
  cl_assert(head == &a);
  cl_assert(a.next == &b);
  cl_assert(b.next == &c);
  cl_assert(c.next == NULL);
}

void test_slist__prepend_null(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode *head = slist_prepend(NULL, &a);
  cl_assert(head == &a);
  cl_assert(a.next == NULL);
}

void test_slist__append(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  SingleListNode *tail;
  tail = slist_append(&a, &b);
  cl_assert(tail == &b);
  tail = slist_append(&a, &c);
  cl_assert(tail == &c);
  cl_assert(a.next == &b);
  cl_assert(b.next == &c);
  cl_assert(c.next == NULL);
}

void test_slist__append_null(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode *tail = slist_append(NULL, &a);
  cl_assert(tail == &a);
}

void test_slist__pop_head(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL, b = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  SingleListNode *new_head = slist_pop_head(&a);
  cl_assert(new_head == &b);
  cl_assert(a.next == NULL);
  cl_assert(b.next == NULL);
}

void test_slist__pop_head_single(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode *new_head = slist_pop_head(&a);
  cl_assert(new_head == NULL);
}

void test_slist__pop_head_null(void) {
  cl_assert(slist_pop_head(NULL) == NULL);
}

void test_slist__remove_head(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  b.next = &c;
  SingleListNode *head = &a;
  slist_remove(&a, &head);
  cl_assert(head == &b);
  cl_assert(a.next == NULL);
  cl_assert(b.next == &c);
}

void test_slist__remove_middle(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  b.next = &c;
  SingleListNode *head = &a;
  slist_remove(&b, &head);
  cl_assert(head == &a);
  cl_assert(a.next == &c);
  cl_assert(b.next == NULL);
  cl_assert(c.next == NULL);
}

void test_slist__remove_tail(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  b.next = &c;
  SingleListNode *head = &a;
  slist_remove(&c, &head);
  cl_assert(head == &a);
  cl_assert(a.next == &b);
  cl_assert(b.next == NULL);
  cl_assert(c.next == NULL);
}

void test_slist__remove_only(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode *head = &a;
  slist_remove(&a, &head);
  cl_assert(head == NULL);
  cl_assert(a.next == NULL);
}

void test_slist__remove_not_found(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode orphan = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  SingleListNode *head = &a;
  slist_remove(&orphan, &head);
  // List should be unchanged
  cl_assert(head == &a);
  cl_assert(a.next == &b);
  cl_assert(b.next == NULL);
}

void test_slist__get_next(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL, b = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  cl_assert(slist_get_next(&a) == &b);
  cl_assert(slist_get_next(&b) == NULL);
  cl_assert(slist_get_next(NULL) == NULL);
}

void test_slist__get_tail(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  b.next = &c;
  cl_assert(slist_get_tail(&a) == &c);
  cl_assert(slist_get_tail(&b) == &c);
  cl_assert(slist_get_tail(&c) == &c);
  cl_assert(slist_get_tail(NULL) == NULL);
}

void test_slist__is_tail(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL, b = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  cl_assert(!slist_is_tail(&a));
  cl_assert(slist_is_tail(&b));
  cl_assert(!slist_is_tail(NULL));
}

void test_slist__count(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  b.next = &c;
  cl_assert_equal_i(slist_count(&a), 3);
  cl_assert_equal_i(slist_count(&b), 2);
  cl_assert_equal_i(slist_count(&c), 1);
  cl_assert_equal_i(slist_count(NULL), 0);
}

void test_slist__contains(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  SingleListNode orphan = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  b.next = &c;
  cl_assert(slist_contains(&a, &a));
  cl_assert(slist_contains(&a, &b));
  cl_assert(slist_contains(&a, &c));
  cl_assert(!slist_contains(&a, &orphan));
  cl_assert(!slist_contains(NULL, &a));
  cl_assert(!slist_contains(&a, NULL));
}

typedef struct SIntNode {
  SingleListNode list_node;
  int value;
} SIntNode;

static bool prv_filter_value(SingleListNode *node, void *data) {
  SIntNode *int_node = (SIntNode *)node;
  return int_node->value == (int)(intptr_t)data;
}

void test_slist__find(void) {
  SIntNode a = { .value = 10 };
  SIntNode b = { .value = 20 };
  SIntNode c = { .value = 30 };
  slist_init(&a.list_node);
  slist_init(&b.list_node);
  slist_init(&c.list_node);
  a.list_node.next = &b.list_node;
  b.list_node.next = &c.list_node;

  cl_assert(slist_find(&a.list_node, prv_filter_value, (void *)(intptr_t)20) == &b.list_node);
  cl_assert(slist_find(&a.list_node, prv_filter_value, (void *)(intptr_t)30) == &c.list_node);
  cl_assert(slist_find(&a.list_node, prv_filter_value, (void *)(intptr_t)10) == &a.list_node);
  cl_assert(slist_find(&a.list_node, prv_filter_value, (void *)(intptr_t)99) == NULL);
  cl_assert(slist_find(NULL, prv_filter_value, (void *)(intptr_t)10) == NULL);
}

static int prv_sort_comparator(SIntNode *a, SIntNode *b) {
  return b->value - a->value;
}

void test_slist__sort_ascending(void) {
  SIntNode bar1 = { .value = 1 };
  SIntNode bar2 = { .value = 2 };
  SIntNode bar3 = { .value = 3 };
  slist_init(&bar1.list_node);
  slist_init(&bar2.list_node);
  slist_init(&bar3.list_node);

  SingleListNode *head = NULL;

  head = slist_sorted_add(head, &bar2.list_node, (Comparator)prv_sort_comparator, true);
  cl_assert(head == &bar2.list_node);

  head = slist_sorted_add(head, &bar3.list_node, (Comparator)prv_sort_comparator, true);
  cl_assert(head == &bar2.list_node);
  cl_assert(slist_get_tail(head) == &bar3.list_node);

  head = slist_sorted_add(head, &bar1.list_node, (Comparator)prv_sort_comparator, true);
  cl_assert(head == &bar1.list_node);
  cl_assert(slist_get_next(head) == &bar2.list_node);
  cl_assert(slist_get_tail(head) == &bar3.list_node);
}

void test_slist__sort_descending(void) {
  SIntNode bar1 = { .value = 1 };
  SIntNode bar2 = { .value = 2 };
  SIntNode bar3 = { .value = 3 };
  slist_init(&bar1.list_node);
  slist_init(&bar2.list_node);
  slist_init(&bar3.list_node);

  SingleListNode *head = NULL;

  head = slist_sorted_add(head, &bar2.list_node, (Comparator)prv_sort_comparator, false);
  cl_assert(head == &bar2.list_node);

  head = slist_sorted_add(head, &bar3.list_node, (Comparator)prv_sort_comparator, false);
  cl_assert(head == &bar3.list_node);
  cl_assert(slist_get_tail(head) == &bar2.list_node);

  head = slist_sorted_add(head, &bar1.list_node, (Comparator)prv_sort_comparator, false);
  cl_assert(head == &bar3.list_node);
  cl_assert(slist_get_next(head) == &bar2.list_node);
  cl_assert(slist_get_tail(head) == &bar1.list_node);
}

void test_slist__concatenate(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  SingleListNode d = SINGLE_LIST_NODE_NULL;
  SingleListNode e = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  b.next = &c;
  d.next = &e;

  SingleListNode *head = slist_concatenate(&a, &d);
  cl_assert(head == &a);
  cl_assert(c.next == &d);
  cl_assert(d.next == &e);
  cl_assert(e.next == NULL);
  cl_assert_equal_i(slist_count(head), 5);
}

void test_slist__concatenate_null(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  cl_assert(slist_concatenate(NULL, &a) == &a);
  cl_assert(slist_concatenate(&a, NULL) == &a);
  cl_assert(slist_concatenate(NULL, NULL) == NULL);
}

#define CTX_VALUE 0xdeadbeef
#define INT_VALUE 17

static bool prv_slist_set_val_each(SingleListNode *node, void *context) {
  SIntNode *int_node = (SIntNode *)node;
  int_node->value = INT_VALUE;
  cl_assert_equal_i(CTX_VALUE, (uintptr_t)context);
  return true;
}

void test_slist__each(void) {
  SIntNode a = {}, b = {}, c = {};
  SingleListNode *head;
  head = slist_prepend((SingleListNode *)&c, (SingleListNode *)&b);
  head = slist_prepend((SingleListNode *)&b, (SingleListNode *)&a);

  cl_assert_equal_i(slist_count(head), 3);
  slist_foreach(head, prv_slist_set_val_each, (void *)(uintptr_t)CTX_VALUE);

  uint32_t num_nodes = 0;
  SingleListNode *temp = head;
  while (temp) {
    SingleListNode *next = temp->next;
    SIntNode *int_node = (SIntNode *)temp;
    cl_assert_equal_i(int_node->value, INT_VALUE);
    temp = next;
    num_nodes++;
  }

  cl_assert_equal_i(num_nodes, 3);
}

static bool prv_stop_at_second(SingleListNode *node, void *context) {
  int *count = (int *)context;
  (*count)++;
  return (*count < 2);
}

void test_slist__each_early_stop(void) {
  SingleListNode a = SINGLE_LIST_NODE_NULL;
  SingleListNode b = SINGLE_LIST_NODE_NULL;
  SingleListNode c = SINGLE_LIST_NODE_NULL;
  a.next = &b;
  b.next = &c;
  int count = 0;
  slist_foreach(&a, prv_stop_at_second, &count);
  cl_assert_equal_i(count, 2);
}
