/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/list.h"

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

void test_list__initialize(void) {
}

void test_list__cleanup(void) {
}

void test_list__insert_after(void) {
  ListNode *tail = NULL;
  ListNode a = LIST_NODE_NULL, b = LIST_NODE_NULL;
  tail = list_insert_after(tail, &a);
  cl_assert(tail == &a);
  tail = list_insert_after(&a, &b);
  cl_assert(tail == &b);
}

void test_list__insert_before(void) {
  ListNode *head = NULL;
  ListNode a = LIST_NODE_NULL, b = LIST_NODE_NULL;
  head = list_insert_before(head, &a);
  cl_assert(head == &a);
  head = list_insert_before(&b, &a);
  cl_assert(head == &a);
}

void test_list__pop_head(void) {
  ListNode a = LIST_NODE_NULL, b = LIST_NODE_NULL;
  list_insert_after(&a, &b);
  ListNode *new_head = list_pop_head(&b);
  cl_assert(new_head == &b);
  cl_assert(list_get_next(&a) == NULL);
  cl_assert(list_get_head(&b) == &b);
}

void test_list__pop_tail(void) {
  ListNode a = LIST_NODE_NULL, b = LIST_NODE_NULL;
  list_insert_after(&a, &b);
  ListNode *new_tail = list_pop_tail(&a);
  cl_assert(new_tail == &a);
  cl_assert(list_get_prev(&b) == NULL);
  cl_assert(list_get_tail(&b) == &b);
}

void test_list__append(void) {
  ListNode a = LIST_NODE_NULL, b = LIST_NODE_NULL, c = LIST_NODE_NULL;
  ListNode *tail;
  tail = list_append(&a, &b);
  cl_assert(tail == &b);
  tail = list_append(&a, &c);
  cl_assert(tail == &c);
  cl_assert(list_get_prev(&a) == NULL);
  cl_assert(list_get_next(&a) == &b);
  cl_assert(list_get_prev(&b) == &a);
  cl_assert(list_get_next(&b) == &c);
  cl_assert(list_get_prev(&c) == &b);
  cl_assert(list_get_next(&c) == NULL);
}

void test_list__prepend(void) {
  ListNode a = LIST_NODE_NULL, b = LIST_NODE_NULL, c = LIST_NODE_NULL;
  ListNode *head;
  head = list_prepend(&c, &b);
  cl_assert(head == &b);
  head = list_prepend(&b, &a);
  cl_assert(head == &a);
  cl_assert(list_get_prev(&a) == NULL);
  cl_assert(list_get_next(&a) == &b);
  cl_assert(list_get_prev(&b) == &a);
  cl_assert(list_get_next(&b) == &c);
  cl_assert(list_get_prev(&c) == &b);
  cl_assert(list_get_next(&c) == NULL);
}

void test_list__count(void) {
  ListNode a = LIST_NODE_NULL, b = LIST_NODE_NULL, c = LIST_NODE_NULL;
  ListNode *tail = list_append(list_append(&a, &b), &c);
  cl_assert(list_count(tail) == 3);
  cl_assert(list_count(&a) == 3);
  cl_assert(list_count(&b) == 3);
  cl_assert(list_count(&c) == 3);
  cl_assert(list_count_to_tail_from(&a) == 3);
  cl_assert(list_count_to_tail_from(&b) == 2);
  cl_assert(list_count_to_tail_from(&c) == 1);
  cl_assert(list_count_to_head_from(&c) == 3);
  cl_assert(list_count_to_head_from(&b) == 2);
  cl_assert(list_count_to_head_from(&a) == 1);
}

typedef struct IntNode {
  ListNode list_node;
  int value;
} IntNode;

int sorting_comparator(IntNode* a, IntNode* b) {
  return b->value - a->value;
}

void test_list__sort_ascending(void) {
  IntNode bar1 = { .value = 1 };
  IntNode bar2 = { .value = 2 };
  IntNode bar3 = { .value = 3 };

  ListNode* head = 0;

  head = list_sorted_add(head, &bar2.list_node, (Comparator) sorting_comparator, true);
  cl_assert(head == &bar2.list_node);

  head = list_sorted_add(head, &bar3.list_node, (Comparator) sorting_comparator, true);
  cl_assert(head == &bar2.list_node);
  cl_assert(list_get_tail(head) == &bar3.list_node);

  head = list_sorted_add(head, &bar1.list_node, (Comparator) sorting_comparator, true);
  cl_assert(head == &bar1.list_node);
  cl_assert(list_get_next(head) == &bar2.list_node);
  cl_assert(list_get_tail(head) == &bar3.list_node);
}

void test_list__sort_descending(void) {
  IntNode bar1 = { .value = 1 };
  IntNode bar2 = { .value = 2 };
  IntNode bar3 = { .value = 3 };

  ListNode* head = 0;

  head = list_sorted_add(head, &bar2.list_node, (Comparator) sorting_comparator, false);
  cl_assert(head == &bar2.list_node);

  head = list_sorted_add(head, &bar3.list_node, (Comparator) sorting_comparator, false);
  cl_assert(head == &bar3.list_node);
  cl_assert(list_get_tail(head) == &bar2.list_node);

  head = list_sorted_add(head, &bar1.list_node, (Comparator) sorting_comparator, false);
  cl_assert(head == &bar3.list_node);
  cl_assert(list_get_next(head) == &bar2.list_node);
  cl_assert(list_get_tail(head) == &bar1.list_node);
}

static bool is_odd(IntNode *node, void *data) {
  return (node->value & 1);
  (void)data;
}

static bool is_even(IntNode *node, void *data) {
  return ((node->value & 1) == 0);
  (void)data;
}

void test_list__find_next_and_prev(void) {
  IntNode bar[5] = {0};
  ListNode* tail = NULL;
  for (int i = 0; i < 5; ++i) {
    bar[i].value = i;
    tail = list_append(tail, &bar[i].list_node);
  }
  bool(*filter_odd)(ListNode*, void *) = (bool(*)(ListNode*, void *)) is_odd;
  bool(*filter_even)(ListNode*, void *) = (bool(*)(ListNode*, void *)) is_even;
  // Find next odd one after '2':
  cl_assert(list_find_next(&bar[2].list_node, filter_odd, false, NULL) == &bar[3].list_node);
  // 5 is the last odd number, so NULL is next:
  cl_assert(list_find_next(&bar[4].list_node, filter_odd, false, NULL) == NULL);
  // Test wrap around, find '1' after '4':
  cl_assert(list_find_next(&bar[4].list_node, filter_odd, true, NULL) == &bar[1].list_node);
  // Test wrap around matching first item, find '0' after '4':
  cl_assert(list_find_next(&bar[4].list_node, filter_even, true, NULL) == &bar[0].list_node);
  // Find prev odd one before '2':
  cl_assert(list_find_prev(&bar[2].list_node, filter_odd, false, NULL) == &bar[1].list_node);
  // '1' is the first odd number, so NULL is prev:
  cl_assert(list_find_prev(&bar[1].list_node, filter_odd, false, NULL) == NULL);
  // Test wrap around, find '3' before '0':
  cl_assert(list_find_prev(&bar[0].list_node, filter_odd, true, NULL) == &bar[3].list_node);
  // Test wrap around matching last item, find '4' before '0':
  cl_assert(list_find_prev(&bar[0].list_node, filter_even, true, NULL) == &bar[4].list_node);

  // Make everything even:
  for (int i = 0; i < 5; ++i) {
    bar[i].value = i * 2;
  }
  // Wrap around once, find nothing and return NULL:
  cl_assert(list_find_next(&bar[3].list_node, (bool(*)(ListNode*, void*))is_odd, true, NULL) == NULL);
  cl_assert(list_find_prev(&bar[3].list_node, (bool(*)(ListNode*, void*))is_odd, true, NULL) == NULL);

  // Test NULL starting node:
  cl_assert(list_find_next(NULL, filter_odd, false, NULL) == NULL);
  cl_assert(list_find_prev(NULL, filter_odd, false, NULL) == NULL);
}

void test_list__concatenate(void) {
  ListNode a = LIST_NODE_NULL, b = LIST_NODE_NULL, c = LIST_NODE_NULL;
  ListNode d = LIST_NODE_NULL, e = LIST_NODE_NULL, f = LIST_NODE_NULL;

  cl_assert_equal_p(list_concatenate(&a, &b), &a);
  cl_assert_equal_p(a.next, &b);
  cl_assert_equal_p(b.prev, &a);

  cl_assert_equal_p(list_concatenate(&b, &c), &a);
  cl_assert_equal_p(b.next, &c);
  cl_assert_equal_p(c.prev, &b);

  cl_assert_equal_p(list_concatenate(&e, &f), &e);
  cl_assert_equal_p(list_concatenate(&d, &f), &d);

  cl_assert_equal_p(list_concatenate(&f, &d), &d);
  cl_assert_equal_p(list_concatenate(NULL, &d), &d);
  cl_assert_equal_p(list_concatenate(NULL, &f), &d);
  cl_assert_equal_p(list_concatenate(&f, NULL), &d);
  cl_assert_equal_p(list_concatenate(&d, NULL), &d);

  cl_assert_equal_p(list_concatenate(&a, &d), &a);
  cl_assert_equal_p(list_get_head(&e), &a);
  cl_assert_equal_p(list_get_tail(&b), &f);

  c.next = NULL;
  d.prev = NULL;

  cl_assert_equal_p(list_concatenate(&c, &f), &a);
  cl_assert_equal_p(list_get_head(&e), &a);
  cl_assert_equal_p(list_get_tail(&b), &f);
}

#define CTX_VALUE 0xdeadbeef
#define INT_VALUE 17

static bool prv_list_set_val_each(ListNode *node, void *context) {
  IntNode *int_node = (IntNode *)node;
  int_node->value = INT_VALUE;
  cl_assert_equal_i(CTX_VALUE, (uintptr_t) context);
  return true;
}

void test_list__each(void) {
  IntNode a = {}, b = {}, c = {};
  ListNode *head;
  head = list_prepend((ListNode *)&c, (ListNode *)&b);
  head = list_prepend((ListNode *)&b, (ListNode *)&a);

  cl_assert_equal_i(list_count((ListNode *)head), 3);
  list_foreach(head, prv_list_set_val_each, (void *)(uintptr_t)CTX_VALUE);

  uint32_t num_nodes = 0;
  ListNode *temp = head;
  while (temp) {
    ListNode *next = temp->next;
    IntNode *int_node = (IntNode *)temp;
    cl_assert_equal_i(int_node->value, INT_VALUE);
    temp = next;
    num_nodes++;
  }

  cl_assert_equal_i(num_nodes, 3);
}
