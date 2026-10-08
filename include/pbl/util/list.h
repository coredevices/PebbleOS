/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "order.h"
#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_list Linked list
 * @ingroup util
 * @brief Intrusive doubly linked list.
 *
 * A list is a chain of ListNode embedded in the caller's structures; there is no separate list
 * object, a list is referenced by any of its nodes, usually its head. Functions that take "any
 * node" walk to the head or tail as needed. Nothing is allocated and there is no locking.
 *
 * @code{.c}
 * struct job {
 *   ListNode node;
 *   int id;
 * };
 *
 * static ListNode *s_jobs;
 *
 * s_jobs = list_prepend(s_jobs, &job->node);
 *
 * for (ListNode *n = s_jobs; n != NULL; n = list_get_next(n)) {
 *   struct job *j = container_of(n, struct job, node);
 *   ...
 * }
 *
 * list_remove(&job->node, &s_jobs, NULL);
 * @endcode
 * @{
 */

/** @brief List node, embedded in the listed structure. */
typedef struct PBL_PACKED ListNode {
  /** Next node, towards the tail, or NULL. */
  struct ListNode *next;
  /** Previous node, towards the head, or NULL. */
  struct ListNode *prev;
} ListNode;

/**
 * @brief Filter for list_find() and its variants.
 *
 * @param found_node Node to check.
 * @param data Callback data.
 * @return true if @p found_node matches.
 */
typedef bool (*ListFilterCallback)(ListNode *found_node, void *data);

/**
 * @brief Callback for list_foreach().
 *
 * The callback may unlink or free @p node.
 *
 * @param node Current node.
 * @param context Callback data.
 * @return true to continue iterating, false to stop.
 */
typedef bool (*ListForEachCallback)(ListNode *node, void *context);

/** @brief Initializer of an unlinked node. */
#define LIST_NODE_NULL {.next = NULL, .prev = NULL}

/**
 * @brief Initialize a node as unlinked.
 *
 * @param[out] head Node.
 */
void list_init(ListNode *head);

/**
 * @brief Insert a node after another one.
 *
 * @param node Node to insert after, may be NULL.
 * @param new_node Node to insert.
 * @return @p new_node.
 */
ListNode *list_insert_after(ListNode *node, ListNode *new_node);

/**
 * @brief Insert a node before another one.
 *
 * @param node Node to insert before, may be NULL.
 * @param new_node Node to insert.
 * @return @p new_node, which is the new head only when @p node was the head.
 */
ListNode *list_insert_before(ListNode *node, ListNode *new_node);

/**
 * @brief Unlink the head of a list.
 *
 * @param node Any node in the list, may be NULL.
 * @return New head, or NULL if the list is now empty.
 */
ListNode *list_pop_head(ListNode *node);

/**
 * @brief Unlink the tail of a list.
 *
 * @param node Any node in the list, may be NULL.
 * @return New tail, or NULL if the list is now empty.
 */
ListNode *list_pop_tail(ListNode *node);

/**
 * @brief Unlink a node from its list.
 *
 * @param node Node to remove, may be NULL.
 * @param[in,out] head Head of the list, updated if it is @p node. May be NULL.
 * @param[in,out] tail Tail of the list, updated if it is @p node. May be NULL.
 */
void list_remove(ListNode *node, ListNode **head, ListNode **tail);

/**
 * @brief Append a node to the tail of a list.
 *
 * @param node Any node in the list, or NULL for an empty list.
 * @param new_node Node to append.
 * @return @p new_node, the new tail.
 */
ListNode *list_append(ListNode *node, ListNode *new_node);

/**
 * @brief Prepend a node to the head of a list.
 *
 * @param node Any node in the list, or NULL for an empty list.
 * @param new_node Node to prepend.
 * @return @p new_node, the new head.
 */
ListNode *list_prepend(ListNode *node, ListNode *new_node);

/**
 * @brief Get the next node.
 *
 * @param node Node, may be NULL.
 * @return Next node, or NULL.
 */
ListNode *list_get_next(ListNode *node);

/**
 * @brief Get the previous node.
 *
 * @param node Node, may be NULL.
 * @return Previous node, or NULL.
 */
ListNode *list_get_prev(ListNode *node);

/**
 * @brief Get the tail of a list.
 *
 * @param node Any node in the list, may be NULL.
 * @return Tail, or NULL for NULL.
 */
ListNode *list_get_tail(ListNode *node);

/**
 * @brief Get the head of a list.
 *
 * @param node Any node in the list, may be NULL.
 * @return Head, or NULL for NULL.
 */
ListNode *list_get_head(ListNode *node);

/**
 * @brief Check whether a node is the head of its list.
 *
 * @param node Node, may be NULL.
 * @return true if @p node has no previous node, false for NULL.
 */
bool list_is_head(const ListNode *node);

/**
 * @brief Check whether a node is the tail of its list.
 *
 * @param node Node, may be NULL.
 * @return true if @p node has no next node, false for NULL.
 */
bool list_is_tail(const ListNode *node);

/**
 * @brief Count the nodes from a node to the tail.
 *
 * @param node Starting node, counted. May be NULL.
 * @return Number of nodes.
 */
uint32_t list_count_to_tail_from(ListNode *node);

/**
 * @brief Count the nodes from a node to the head.
 *
 * @param node Starting node, counted. May be NULL.
 * @return Number of nodes.
 */
uint32_t list_count_to_head_from(ListNode *node);

/**
 * @brief Count the nodes of a list.
 *
 * @param node Any node in the list, may be NULL.
 * @return Number of nodes.
 */
uint32_t list_count(ListNode *node);

/**
 * @brief Get the node at a distance from another one.
 *
 * @param node Starting node.
 * @param index Number of nodes to move, towards the tail if positive, the head if negative.
 * @return Node found, or NULL if the list ends first.
 */
ListNode *list_get_at(ListNode *node, int32_t index);

/**
 * @brief Insert a node into a sorted list, keeping it sorted.
 *
 * Existing nodes are not sorted. The node goes before the first node that sorts after it, so
 * equal nodes keep their insertion order.
 *
 * @param head Head of the list, or NULL for an empty list.
 * @param new_node Node to insert.
 * @param comparator Called with an existing node and @p new_node; see Comparator.
 * @param ascending true to keep the list in ascending order from head to tail.
 * @return New head of the list.
 */
ListNode *list_sorted_add(ListNode *head, ListNode *new_node, Comparator comparator,
                          bool ascending);

/**
 * @brief Check whether a list contains a node.
 *
 * @param head Head of the list, may be NULL.
 * @param node Node to search for.
 * @return true if @p node is at or after @p head.
 */
bool list_contains(const ListNode *head, const ListNode *node);

/**
 * @brief Find the first matching node, from a node towards the tail.
 *
 * @param node Node to start from, included in the search. May be NULL.
 * @param filter_callback Filter.
 * @param data Filter data.
 * @return Matching node, or NULL.
 */
ListNode *list_find(ListNode *node, ListFilterCallback filter_callback, void *data);

/**
 * @brief Find the next matching node after a node.
 *
 * @param node Node to start after. May be NULL.
 * @param filter_callback Filter.
 * @param wrap_around Continue from the head after reaching the tail, up to and including
 * @p node.
 * @param data Filter data.
 * @return Matching node, or NULL.
 */
ListNode *list_find_next(ListNode *node, ListFilterCallback filter_callback, bool wrap_around,
                         void *data);

/**
 * @brief Find the previous matching node before a node.
 *
 * @param node Node to start before. May be NULL.
 * @param filter_callback Filter.
 * @param wrap_around Continue from the tail after reaching the head, up to and including
 * @p node.
 * @param data Filter data.
 * @return Matching node, or NULL.
 */
ListNode *list_find_prev(ListNode *node, ListFilterCallback filter_callback, bool wrap_around,
                         void *data);

/**
 * @brief Append a list to another one.
 *
 * Nothing is done when both nodes are already in the same list.
 *
 * @param list_a Any node of the first list, may be NULL.
 * @param list_b Any node of the list to append, may be NULL.
 * @return Head of the resulting list.
 */
ListNode *list_concatenate(ListNode *list_a, ListNode *list_b);

/**
 * @brief Call a function on each node, from a node to the tail.
 *
 * @param head Starting node, may be NULL.
 * @param each_cb Callback; it may unlink or free the node it gets.
 * @param context Callback data.
 */
void list_foreach(ListNode *head, ListForEachCallback each_cb, void *context);

/**
 * @brief Log every node from a node to the tail with UTIL_LOG().
 *
 * @param head Starting node.
 */
void list_debug_dump(ListNode *head);

/** @} */
