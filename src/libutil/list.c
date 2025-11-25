/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/list.h"
#include "util/assert.h"
#include "util/logging.h"

#include <stddef.h>
#include <stdio.h>

void list_init(ListNode* node) {
  node->next = NULL;
  node->prev = NULL;
}

ListNode* list_insert_after(ListNode* node, ListNode* new_node) {
  if (node == NULL) {
    return new_node;
  }
  new_node->next = node->next;
  new_node->prev = node;

  if (node->next) {
    node->next->prev = new_node;
  }
  node->next = new_node;

  return new_node;
}

ListNode* list_insert_before(ListNode* node, ListNode* new_node) {
  if (node == NULL) {
    return new_node;
  }
  new_node->next = node;
  new_node->prev = node->prev;

  if (node->prev) {
    node->prev->next = new_node;
  }
  node->prev = new_node;

  return new_node;
}

ListNode* list_pop_head(ListNode *node) {
  if (node == NULL) {
    return NULL;
  }
  ListNode *head = list_get_head(node);
  ListNode *new_head = head->next;
  list_remove(head, NULL, NULL);
  return new_head;
}

ListNode* list_pop_tail(ListNode *node) {
  if (node == NULL) {
    return NULL;
  }
  ListNode* tail = list_get_tail(node);
  ListNode* new_tail = tail->prev;
  list_remove(tail, NULL, NULL);
  return new_tail;
}

void list_remove(ListNode* node, ListNode **head, ListNode **tail) {
  if (node == NULL) {
    return;
  }
  if (head && *head == node) {
    *head = node->next;
  }
  if (tail && *tail == node) {
    *tail = node->prev;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }
  if (node->prev) {
    node->prev->next = node->next;
  }
  node->prev = NULL;
  node->next = NULL;
}

ListNode* list_append(ListNode* node, ListNode* new_node) {
  return list_insert_after(list_get_tail(node), new_node);
}

ListNode* list_prepend(ListNode* node, ListNode* new_node) {
  return list_insert_before(list_get_head(node), new_node);
}

ListNode* list_get_next(ListNode* node) {
  if (node == NULL) {
    return NULL;
  }
  return node->next;
}

ListNode* list_get_prev(ListNode* node) {
  if (node == NULL) {
    return NULL;
  }
  return node->prev;
}

ListNode* list_get_tail(ListNode* node) {
  if (node == NULL) {
    return NULL;
  }
  while (node->next != NULL) {
    node = node->next;
  }
  return node;
}

ListNode* list_get_head(ListNode* node) {
  if (node == NULL) {
    return NULL;
  }
  while (node->prev != NULL) {
    node = node->prev;
  }
  return node;
}

bool list_is_head(const ListNode *node) {
  if (!node) {
    return false;
  }
  return !node->prev;
}

bool list_is_tail(const ListNode *node) {
  if (!node) {
    return false;
  }
  return !node->next;
}

uint32_t list_count_to_tail_from(ListNode* node) {
  if (node == NULL) {
    return 0;
  }
  uint32_t count = 1;
  while ((node = node->next) != NULL) {
    ++count;
  }
  return count;
}

uint32_t list_count_to_head_from(ListNode *node) {
  if (node == NULL) {
    return 0;
  }
  uint32_t count = 1;
  while ((node = node->prev) != NULL) {
    ++count;
  }
  return count;
}

uint32_t list_count(ListNode* node) {
  return list_count_to_tail_from(list_get_head(node));
}

ListNode* list_get_at(ListNode *node, int32_t index) {
  while (node != NULL && index != 0) {
    if (index > 0) {
      node = node->next;
      index--;
    } else {
      node = node->prev;
      index++;
    }
  }
  return node;
}

ListNode* list_sorted_add(ListNode *node, ListNode *new_node, Comparator comparator, bool ascending) {
  if (node == NULL) {
    return new_node;
  }
  if (new_node == NULL) {
    return node;
  }
  ListNode * const head = node;
  for(;;) {
    int order = comparator(node, new_node);
    if (!ascending) {
      order = -order;
    }

    if (order < 0) {
      list_insert_before(node, new_node);
      if (node == head) {
        return new_node;
      } else {
        return head;
      }
    }
    ListNode *next = node->next;
    if (next == NULL) {
      list_insert_after(node, new_node);
      return head;
    }
    node = next;
  }
}

bool list_contains(const ListNode *node, const ListNode *node_to_search) {
  if (node == NULL || node_to_search == NULL) {
    return false;
  }
  while (node) {
    if (node == node_to_search) {
      return true;
    }
    node = node->next;
  }
  return false;
}

ListNode* list_find(ListNode *node, ListFilterCallback filter_callback, void *data) {
  if (node == NULL) {
    return NULL;
  }
  ListNode *cursor = node;
  do {
    if (filter_callback(cursor, data)) {
      return cursor;
    }
  } while ((cursor = cursor->next));
  return NULL;
}

ListNode* list_find_next(ListNode *node, ListFilterCallback filter_callback, bool wrap_around, void *data) {
  if (node == NULL) {
    return NULL;
  }
  ListNode *cursor = node;
  while ((cursor = cursor->next)) {
    if (filter_callback(cursor, data)) {
      return cursor;
    }
  }
  if (wrap_around == false) {
    return NULL;
  }
  cursor = list_get_head(node);
  while (cursor) {
    if (filter_callback(cursor, data)) {
      return cursor;
    }
    // We're back at where we started and even <node> itself doesn't match the filter
    if (cursor == node) {
      return NULL;
    }
    cursor = cursor->next;
  }
  UTIL_ASSERT(0);
  return NULL;
}

ListNode* list_find_prev(ListNode *node, ListFilterCallback filter_callback, bool wrap_around, void *data) {
  if (node == NULL) {
    return NULL;
  }
  ListNode *cursor = node;
  while ((cursor = cursor->prev)) {
    if (filter_callback(cursor, data)) {
      return cursor;
    }
  }
  if (wrap_around == false) {
    return NULL;
  }
  cursor = list_get_tail(node);
  while (cursor) {
    if (filter_callback(cursor, data)) {
      return cursor;
    }
    // We're back at where we started and even <node> itself doesn't match the filter
    if (cursor == node) {
      return NULL;
    }
    cursor = cursor->prev;
  }
  UTIL_ASSERT(0);
  return NULL;
}

ListNode *list_concatenate(ListNode *restrict list_a, ListNode *restrict list_b) {
  ListNode *head_a = list_get_head(list_a);
  if (list_b == NULL) {
    return head_a;
  }

  ListNode *head_b = list_get_head(list_b);
  if (list_a == NULL) {
    return head_b;
  }

  if (head_a == head_b) {
    // list b is already in list a!
    return head_a;
  }

  ListNode *tail_a = list_get_tail(list_a);

  head_b->prev = tail_a;
  tail_a->next = head_b;

  return head_a;
}

void list_foreach(ListNode *head, ListForEachCallback each_cb, void *context) {
  if (!each_cb) {
    return;
  }

  ListNode *iter = head;
  while (iter) {
    // Save off a pointer so the client to this function can destroy the node (useful for deinits)
    ListNode *next = iter->next;
    if (!each_cb(iter, context)) {
      return;
    }
    iter = next;
  }
}

void list_debug_dump(ListNode *head) {
  ListNode *iter = head;
  char buffer[30];
  while (iter) {
    snprintf(buffer, sizeof(buffer), "node %p (%p, %p)", iter, iter->prev, iter->next);
    UTIL_LOG(buffer);
    iter = iter->next;
  }
}

