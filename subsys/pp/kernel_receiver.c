/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/slist.h>

#include <kernel/event_loop.h>

PBL_LOG_MODULE_DECLARE(pp, CONFIG_PP_LOG_LEVEL);

struct pbl_pp__kernel_message {
  SingleListNode node;
  struct pbl_pp_session *session;
  const struct pbl_pp_endpoint *endpoint;
  size_t length;
  size_t received;
  bool scheduled;
  uint8_t payload[];
};

// One callback per task drains its list, one message per callback, so that a slow handler
// cannot starve the task watchdog and bursts cannot overflow the task queue.
struct pbl_pp__kernel_queue {
  SingleListNode *head;
  bool callback_pending;
  void (*add_callback)(void (*cb)(void *data), void *data);
};

static void prv_bg_add_callback(void (*cb)(void *data), void *data) {
  pbl_sys_work_call(cb, data);
}

static void prv_main_add_callback(void (*cb)(void *data), void *data) {
  launcher_task_add_callback(cb, data);
}

static PBL_MUTEX_DEFINE(s_lock);

static struct pbl_pp__kernel_queue s_queues[] = {
  [PBL_PP_HANDLER_BG] = {.add_callback = prv_bg_add_callback},
  [PBL_PP_HANDLER_MAIN] = {.add_callback = prv_main_add_callback},
};

static void prv_drain_cb(void *data) {
  struct pbl_pp__kernel_queue *queue = data;
  struct pbl_pp__kernel_message *msg;
  bool more;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  msg = (struct pbl_pp__kernel_message *)queue->head;
  if (msg != nullptr) {
    queue->head = slist_get_next(&msg->node);
  }
  pbl_mutex_unlock(&s_lock);

  if (msg != nullptr) {
    msg->endpoint->handler(msg->session, msg->payload, msg->length);
    pbl_free(msg);
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  more = (queue->head != nullptr);
  queue->callback_pending = more;
  pbl_mutex_unlock(&s_lock);

  if (more) {
    queue->add_callback(prv_drain_cb, queue);
  }
}

static struct pbl_pp_receiver *prv_prepare(struct pbl_pp_session *session,
                                           const struct pbl_pp_endpoint *endpoint, size_t length) {
  struct pbl_pp__kernel_message *msg;

  if (length == 0U) {
    return nullptr;
  }

  msg = pbl_zalloc(sizeof(*msg) + length);
  if (msg == nullptr) {
    PBL_LOG_WRN("Could not allocate %u bytes for endpoint 0x%" PRIx16, (unsigned int)length,
                endpoint->id);
    return nullptr;
  }

  msg->session = session;
  msg->endpoint = endpoint;
  msg->length = length;

  return (struct pbl_pp_receiver *)msg;
}

static void prv_write(struct pbl_pp_receiver *receiver, const uint8_t *data, size_t length) {
  struct pbl_pp__kernel_message *msg = (struct pbl_pp__kernel_message *)receiver;

  PBL_ASSERTN(msg->received + length <= msg->length);
  memcpy(&msg->payload[msg->received], data, length);
  msg->received += length;
}

static void prv_finish(struct pbl_pp_receiver *receiver) {
  struct pbl_pp__kernel_message *msg = (struct pbl_pp__kernel_message *)receiver;
  struct pbl_pp__kernel_queue *queue = &s_queues[msg->endpoint->task];
  bool schedule;

  if (msg->received != msg->length) {
    PBL_LOG_WRN("Got fewer bytes than expected for endpoint 0x%" PRIx16, msg->endpoint->id);
  }

  msg->scheduled = true;
  slist_init(&msg->node);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (queue->head != nullptr) {
    slist_append(queue->head, &msg->node);
  } else {
    queue->head = &msg->node;
  }
  schedule = !queue->callback_pending;
  queue->callback_pending = true;
  pbl_mutex_unlock(&s_lock);

  if (schedule) {
    queue->add_callback(prv_drain_cb, queue);
  }
}

static void prv_cleanup(struct pbl_pp_receiver *receiver) {
  struct pbl_pp__kernel_message *msg = (struct pbl_pp__kernel_message *)receiver;

  if (!msg->scheduled) {
    pbl_free(msg);
  }
}

const struct pbl_pp_receiver_api pbl_pp_kernel_receiver = {
  .prepare = prv_prepare,
  .write = prv_write,
  .finish = prv_finish,
  .cleanup = prv_cleanup,
};
