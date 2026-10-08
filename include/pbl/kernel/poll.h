/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/msgq.h>

/**
 * @defgroup kernel_poll Poll groups
 * @ingroup kernel
 * @brief Waiting on several message queues at once.
 *
 * pbl_poll_group_wait() returns a member queue that has a message without dequeuing it; the
 * caller receives it with pbl_msgq_get(). Members are scanned from the one after the last
 * returned, so a busy queue cannot starve the others. Members may also be read directly, outside
 * the group. Queues are added before the first wait and stay members for good; only one thread
 * may wait on a group.
 *
 * @code{.c}
 * static PBL_MSGQ_DEFINE(s_from_isr, sizeof(struct event), 8);
 * static PBL_MSGQ_DEFINE(s_from_app, sizeof(struct event), 4);
 * static PBL_POLL_GROUP_DEFINE(s_group);
 *
 * static void prv_loop(void *arg) {
 *   pbl_poll_group_add(&s_group, &s_from_isr);
 *   pbl_poll_group_add(&s_group, &s_from_app);
 *
 *   for (;;) {
 *     struct pbl_msgq *q = pbl_poll_group_wait(&s_group, PBL_SEC(1));
 *     if (q == NULL) {
 *       prv_handle_timeout();
 *       continue;
 *     }
 *     struct event e;
 *     if (pbl_msgq_get(q, &e, PBL_NO_WAIT) == 0) {
 *       prv_handle(q, &e);
 *     }
 *   }
 * }
 * @endcode
 * @{
 */

/** @brief Set of message queues waited on together. */
struct pbl_poll_group {
  /** First member queue, in the order they were added. */
  struct pbl_msgq *members;
  /** Sum of the capacities of all members, in messages. */
  uint32_t capacity;
  /** Backend state. */
  struct pbl_poll_group_backend backend;
};

/** @brief Static initializer for an empty group. */
#define PBL_POLL_GROUP_INITIALIZER {.members = NULL, .capacity = 0}

/**
 * @brief Define an empty group, usable without pbl_poll_group_init().
 *
 * @param name Name of the group variable.
 */
#define PBL_POLL_GROUP_DEFINE(name) struct pbl_poll_group name = PBL_POLL_GROUP_INITIALIZER

/**
 * @brief Initialize a group in dynamically allocated memory.
 *
 * @param[out] g Group.
 */
void pbl_poll_group_init(struct pbl_poll_group *g);

/**
 * @brief Add a queue to a group.
 *
 * @param g Group.
 * @param q Queue; must be empty and not already in a group.
 */
void pbl_poll_group_add(struct pbl_poll_group *g, struct pbl_msgq *q);

/**
 * @brief Wait until a member queue has a message.
 *
 * Not callable from ISRs. The group must have at least one member.
 *
 * @param g Group.
 * @param timeout How long to wait.
 * @return A member with a pending message, or NULL on timeout or if the thread was suspended
 *         while waiting.
 */
struct pbl_msgq *pbl_poll_group_wait(struct pbl_poll_group *g, pbl_timeout_t timeout);

/**
 * @brief Check whether every member queue is empty.
 *
 * @param g Group.
 * @return true if no member has a message.
 */
bool pbl_poll_group_is_empty(const struct pbl_poll_group *g);

/** @} */
