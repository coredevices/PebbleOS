/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/types.h>

struct pbl_poll_group;

/**
 * @defgroup kernel_msgq Message queues
 * @ingroup kernel
 * @brief Fixed-size message rings that copy messages in and out.
 *
 * Every message is @ref pbl_msgq::msg_size bytes and is copied, so senders may pass stack
 * variables. Blocked senders and receivers are served highest priority first. Put and get may be
 * called from ISRs, which never block and get @c -EBUSY on a full or empty queue. A queue can be
 * waited on together with others through a @ref kernel_poll "poll group".
 *
 * @code{.c}
 * struct event {
 *   uint8_t type;
 *   uint32_t data;
 * };
 *
 * static PBL_MSGQ_DEFINE(s_events, sizeof(struct event), 8);
 *
 * static void prv_button_isr(void) {
 *   const struct event e = {.type = EVENT_BUTTON, .data = prv_read_buttons()};
 *   pbl_msgq_put(&s_events, &e, PBL_NO_WAIT);
 * }
 *
 * static void prv_event_thread(void *arg) {
 *   struct event e;
 *   for (;;) {
 *     if (pbl_msgq_get(&s_events, &e, PBL_FOREVER) == 0) {
 *       prv_handle(&e);
 *     }
 *   }
 * }
 * @endcode
 * @{
 */

/** @brief Fixed-size message ring. Usable from ISRs with @ref PBL_NO_WAIT. */
struct pbl_msgq {
  /** Storage, @ref msg_size * @ref max_msgs bytes. */
  void *buf;
  /** Size of one message in bytes. */
  size_t msg_size;
  /** Capacity in messages. */
  uint32_t max_msgs;
  /** Poll group the queue belongs to, NULL if none. */
  struct pbl_poll_group *group;
  /** Next member of the same poll group. */
  struct pbl_msgq *group_next;
  /** Backend state. */
  struct pbl_msgq_backend backend;
};

/**
 * @brief Static initializer for an empty queue.
 *
 * @param buffer Storage of at least @p size * @p max bytes.
 * @param size Size of one message in bytes.
 * @param max Capacity in messages.
 */
#define PBL_MSGQ_INITIALIZER(buffer, size, max) \
  {.buf = (buffer), .msg_size = (size), .max_msgs = (max)}

/**
 * @brief Anonymous, word-aligned storage for a queue.
 *
 * A file-scope compound literal has static storage duration, so the buffer needs no name and the
 * definition using it can be prefixed with @c static.
 *
 * @param size Size of one message in bytes.
 * @param max Capacity in messages.
 */
#define PBL_MSGQ_STATIC_BUF(size, max) ((uint32_t[((size) *(max) + 3) / 4]){0})

/**
 * @brief Define a queue with its own storage, usable without pbl_msgq_init().
 *
 * Only valid at file scope.
 *
 * @param name Name of the queue variable.
 * @param size Size of one message in bytes.
 * @param max Capacity in messages.
 */
#define PBL_MSGQ_DEFINE(name, size, max) \
  struct pbl_msgq name = PBL_MSGQ_INITIALIZER(PBL_MSGQ_STATIC_BUF(size, max), size, max)

/**
 * @brief Initialize a queue in dynamically allocated memory.
 *
 * @param[out] q Queue.
 * @param buf Storage of at least @p msg_size * @p max_msgs bytes, owned by the caller.
 * @param msg_size Size of one message in bytes, not 0.
 * @param max_msgs Capacity in messages, not 0.
 */
void pbl_msgq_init(struct pbl_msgq *q, void *buf, size_t msg_size, uint32_t max_msgs);

/**
 * @brief Release a queue before its memory is reused.
 *
 * Required for dynamically allocated queues. Not for poll group members, which cannot leave
 * their group.
 *
 * @param q Queue.
 */
void pbl_msgq_deinit(struct pbl_msgq *q);

/**
 * @brief Append a message.
 *
 * @param q Queue.
 * @param msg Message of @ref pbl_msgq::msg_size bytes, copied in.
 * @param timeout How long to wait for space; ignored in an ISR, which never waits.
 * @retval 0 Queued.
 * @retval -EAGAIN Timed out.
 * @retval -EBUSY Full and @p timeout is @ref PBL_NO_WAIT, or called from an ISR.
 * @retval -EINTR The thread was suspended while waiting.
 */
int pbl_msgq_put(struct pbl_msgq *q, const void *msg, pbl_timeout_t timeout);
/**
 * @brief Prepend a message, so it is received next.
 *
 * @param q Queue.
 * @param msg Message of @ref pbl_msgq::msg_size bytes, copied in.
 * @param timeout How long to wait for space; ignored in an ISR, which never waits.
 * @retval 0 Queued.
 * @retval -EAGAIN Timed out.
 * @retval -EBUSY Full and @p timeout is @ref PBL_NO_WAIT, or called from an ISR.
 * @retval -EINTR The thread was suspended while waiting.
 */
int pbl_msgq_put_front(struct pbl_msgq *q, const void *msg, pbl_timeout_t timeout);
/**
 * @brief Receive the oldest message.
 *
 * @param q Queue.
 * @param[out] msg Buffer of @ref pbl_msgq::msg_size bytes.
 * @param timeout How long to wait for a message; ignored in an ISR, which never waits.
 * @retval 0 Received.
 * @retval -EAGAIN Timed out.
 * @retval -EBUSY Empty and @p timeout is @ref PBL_NO_WAIT, or called from an ISR.
 * @retval -EINTR The thread was suspended while waiting.
 */
int pbl_msgq_get(struct pbl_msgq *q, void *msg, pbl_timeout_t timeout);
/**
 * @brief Copy the oldest message without removing it.
 *
 * @param q Queue.
 * @param[out] msg Buffer of @ref pbl_msgq::msg_size bytes.
 * @retval 0 Copied.
 * @retval -EBUSY Empty.
 */
int pbl_msgq_peek(struct pbl_msgq *q, void *msg);
/**
 * @brief Discard every message.
 *
 * Wakes the threads waiting for space.
 *
 * @param q Queue.
 */
void pbl_msgq_purge(struct pbl_msgq *q);
/**
 * @brief Get the number of queued messages.
 *
 * @param q Queue.
 * @return Messages queued.
 */
uint32_t pbl_msgq_num_used(const struct pbl_msgq *q);
/**
 * @brief Get the free space of a queue.
 *
 * @param q Queue.
 * @return Messages that can be put without waiting.
 */
uint32_t pbl_msgq_num_free(const struct pbl_msgq *q);

/** @} */
