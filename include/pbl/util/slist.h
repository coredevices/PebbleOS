/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "order.h"

#include <stdint.h>

/**
 * @defgroup util_slist Singly linked list
 * @ingroup util
 * @brief Intrusive singly linked list.
 *
 * Like @ref util_list with half the per-node overhead, at the cost of linear-time removal. A list
 * is referenced by its head.
 *
 * @code{.c}
 * struct waiter {
 *   SingleListNode node;
 *   int id;
 * };
 *
 * static SingleListNode *s_waiters;
 *
 * s_waiters = slist_prepend(s_waiters, &w->node);
 *
 * for (SingleListNode *n = s_waiters; n != NULL; n = slist_get_next(n)) {
 *   struct waiter *it = container_of(n, struct waiter, node);
 *   ...
 * }
 *
 * slist_remove(&w->node, &s_waiters);
 * @endcode
 * @{
 */

/** @brief Singly linked list node, embedded in the listed structure. */
typedef struct SingleListNode {
  /** Next node, or NULL. */
  struct SingleListNode *next;
} SingleListNode;

/**
 * @brief Filter for slist_find().
 *
 * @param found_node Node to check.
 * @param data Callback data.
 * @return true if @p found_node matches.
 */
typedef bool (*SingleListFilterCallback)(SingleListNode *found_node, void *data);

/**
 * @brief Callback for slist_foreach().
 *
 * The callback may unlink or free @p node.
 *
 * @param node Current node.
 * @param context Callback data.
 * @return true to continue iterating, false to stop.
 */
typedef bool (*SingleListForEachCallback)(SingleListNode *node, void *context);

/** @brief Initializer of an unlinked node. */
#define SINGLE_LIST_NODE_NULL {.next = NULL}

/**
 * @brief Initialize a node as unlinked.
 *
 * @param[out] node Node.
 */
void slist_init(SingleListNode *node);

/**
 * @brief Insert a node after another one.
 *
 * @param node Node to insert after, may be NULL.
 * @param new_node Node to insert.
 * @return @p new_node.
 */
SingleListNode *slist_insert_after(SingleListNode *node, SingleListNode *new_node);

/**
 * @brief Prepend a node to a list.
 *
 * @param head Head of the list, or NULL for an empty list.
 * @param new_node Node to prepend, may be NULL.
 * @return New head of the list.
 */
SingleListNode *slist_prepend(SingleListNode *head, SingleListNode *new_node);

/**
 * @brief Append a node to the tail of a list.
 *
 * @param head Any node in the list, or NULL for an empty list.
 * @param new_node Node to append.
 * @return @p new_node, the new tail.
 */
SingleListNode *slist_append(SingleListNode *head, SingleListNode *new_node);

/**
 * @brief Unlink the head of a list.
 *
 * @param head Head of the list, may be NULL.
 * @return New head, or NULL if the list is now empty.
 */
SingleListNode *slist_pop_head(SingleListNode *head);

/**
 * @brief Unlink a node from a list.
 *
 * Nothing is done if @p node is not in the list.
 *
 * @param node Node to remove.
 * @param[in,out] head Head of the list, updated if it is @p node.
 */
void slist_remove(SingleListNode *node, SingleListNode **head);

/**
 * @brief Get the next node.
 *
 * @param node Node, may be NULL.
 * @return Next node, or NULL.
 */
SingleListNode *slist_get_next(SingleListNode *node);

/**
 * @brief Get the tail of a list.
 *
 * @param node Any node in the list, may be NULL.
 * @return Tail, or NULL for NULL.
 */
SingleListNode *slist_get_tail(SingleListNode *node);

/**
 * @brief Check whether a node is the tail of its list.
 *
 * @param node Node, may be NULL.
 * @return true if @p node has no next node, false for NULL.
 */
bool slist_is_tail(const SingleListNode *node);

/**
 * @brief Count the nodes of a list.
 *
 * @param head Head of the list, may be NULL.
 * @return Number of nodes.
 */
uint32_t slist_count(SingleListNode *head);

/**
 * @brief Check whether a list contains a node.
 *
 * @param head Head of the list, may be NULL.
 * @param node Node to search for.
 * @return true if @p node is in the list.
 */
bool slist_contains(const SingleListNode *head, const SingleListNode *node);

/**
 * @brief Find the first matching node.
 *
 * @param head Node to start from, included in the search. May be NULL.
 * @param filter_callback Filter.
 * @param data Filter data.
 * @return Matching node, or NULL.
 */
SingleListNode *slist_find(SingleListNode *head, SingleListFilterCallback filter_callback,
                           void *data);

/**
 * @brief Insert a node into a sorted list, keeping it sorted.
 *
 * Existing nodes are not sorted. Equal nodes keep their insertion order.
 *
 * @param head Head of the list, or NULL for an empty list.
 * @param new_node Node to insert.
 * @param comparator Called with an existing node and @p new_node; see Comparator.
 * @param ascending true to keep the list in ascending order from head to tail.
 * @return New head of the list.
 */
SingleListNode *slist_sorted_add(SingleListNode *head, SingleListNode *new_node,
                                 Comparator comparator, bool ascending);

/**
 * @brief Append a list to another one.
 *
 * @param list_a Head of the first list, may be NULL.
 * @param list_b Head of the list to append, may be NULL.
 * @return Head of the resulting list.
 */
SingleListNode *slist_concatenate(SingleListNode *list_a, SingleListNode *list_b);

/**
 * @brief Call a function on each node of a list.
 *
 * @param head Head of the list, may be NULL.
 * @param each_cb Callback; it may unlink or free the node it gets.
 * @param context Callback data.
 */
void slist_foreach(SingleListNode *head, SingleListForEachCallback each_cb, void *context);

/**
 * @brief Log every node of a list with UTIL_LOG().
 *
 * @param head Head of the list.
 */
void slist_debug_dump(SingleListNode *head);

/** @} */
