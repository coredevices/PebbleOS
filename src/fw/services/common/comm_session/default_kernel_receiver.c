/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/common/comm_session/session_receive_router.h"

#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/common/comm_session/session.h"
#include "pbl/services/common/system_task.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/slist.h"

#include <inttypes.h>

//! Default option for the kernel receiver, execute the endpoint handler on KernelBG.
const PebbleTask g_default_kernel_receiver_opt_bg = PebbleTask_KernelBackground;

//! If the endpoint handler puts events onto the KernelMain queue *and* it is important that
//! PEBBLE_COMM_SESSION_EVENT and your endpoint's events are handled in order, use this
//! receiver option in the protocol_endpoints_table.json:
const PebbleTask g_default_kernel_receiver_opt_main = PebbleTask_KernelMain;

// A common pattern for endpoint handlers it to:
//   1) Kernel malloc a buffer & copy Pebble Protocol payload to it
//   2) Schedule a callback on KernelBG/Main to run the code that decodes the payload
//      (because the handler runs from BT02, a high priority thread
//   3) Free malloc'ed buffer
// Leaving this up to each individual endpoint wastes code and creates more
// opportunity for memory leaks. This file contains an implementation that
// different endpoints can use to achieve this pattern.
//
// Note: Since the buffer is malloc'ed on the kernel heap, the expected consumer
//       for this receiver is the system (not an app). However, it might be
//       receiving messages *from* a PebbleKit app that the system is supposed
//       to handle. For example, app run state commands (i.e. "app launch") are
//       sent by PebbleKit apps, but get handled by the system.

typedef struct {
  SingleListNode node;
  CommSession *session;
  const PebbleProtocolEndpoint *endpoint;
  size_t total_payload_size;
  int curr_pos;
  bool handler_scheduled;
  bool should_use_kernel_main;
  uint8_t payload[];
} DefaultReceiverImpl;

//! Pending receiver lists: completed messages are appended here instead of each
//! getting their own system_task/launcher_task callback. A single callback is
//! scheduled per list and drains all entries when it fires. This prevents the
//! system task queue from overflowing when many Pebble Protocol messages arrive
//! in rapid succession (e.g. after BT reconnect).
static SingleListNode *s_pending_bg_head;
static bool s_bg_callback_pending;

static SingleListNode *s_pending_main_head;
static bool s_main_callback_pending;

static Receiver *prv_default_kernel_receiver_prepare(
    CommSession *session, const PebbleProtocolEndpoint *endpoint,
    size_t total_payload_size) {
  if (total_payload_size == 0) {
    return NULL;  // Ignore zero-length messages
  }

  size_t size_needed = sizeof(DefaultReceiverImpl) + total_payload_size;
  DefaultReceiverImpl *receiver = kernel_zalloc(size_needed);

  if (!receiver) {
    PBL_LOG_WRN("Could not allocate receiver, handler:%p size:%d",
            endpoint->handler, (int)size_needed);
    return NULL;
  }

  const bool should_use_kernel_main =
      (endpoint->receiver_opt == &g_default_kernel_receiver_opt_main);
  *receiver = (DefaultReceiverImpl) {
    .session = session,
    .endpoint = endpoint,
    .total_payload_size = total_payload_size,
    .should_use_kernel_main = should_use_kernel_main,
    .curr_pos = 0
  };

  return (Receiver *)receiver;
}

static void prv_default_kernel_receiver_write(
    Receiver *receiver, const uint8_t *data, size_t length) {
  DefaultReceiverImpl *impl = (DefaultReceiverImpl *)receiver;

  PBL_ASSERTN((impl->curr_pos + length) <= impl->total_payload_size);
  memcpy(impl->payload + impl->curr_pos, data, length);

  impl->curr_pos += length;
}

static void prv_wipe_receiver_data(DefaultReceiverImpl *receiver) {
  *receiver = (DefaultReceiverImpl) { };
}

static void prv_append_to_pending_list(DefaultReceiverImpl *impl,
                                       SingleListNode **head) {
  slist_init(&impl->node);
  if (*head) {
    slist_append(*head, &impl->node);
  } else {
    *head = &impl->node;
  }
}

static void prv_drain_pending_list(SingleListNode **head,
                                   bool *callback_pending) {
  // Snapshot and detach the list so new items that arrive while we are
  // processing do not extend the current batch indefinitely.
  SingleListNode *list = *head;
  *head = NULL;
  *callback_pending = false;

  while (list) {
    DefaultReceiverImpl *impl = (DefaultReceiverImpl *)list;
    list = slist_get_next(list);
    PBL_ASSERTN(impl->handler_scheduled && impl->session);
    impl->endpoint->handler(impl->session, impl->payload, impl->total_payload_size);
    prv_wipe_receiver_data(impl);
    kernel_free(impl);
  }
}

static void prv_default_kernel_receiver_bg_cb(void *data) {
  prv_drain_pending_list(&s_pending_bg_head, &s_bg_callback_pending);
}

static void prv_default_kernel_receiver_main_cb(void *data) {
  prv_drain_pending_list(&s_pending_main_head, &s_main_callback_pending);
}

static void prv_default_kernel_receiver_finish(Receiver *receiver) {
  DefaultReceiverImpl *impl = (DefaultReceiverImpl *)receiver;
  impl->handler_scheduled = true;

  if ((int)impl->total_payload_size != impl->curr_pos) {
    PBL_LOG_WRN("Got fewer bytes than expected for handler %p",
            impl->endpoint->handler);
  }

  // Coalesce callbacks: append to the pending list and only schedule a new
  // system_task / launcher_task callback if one isn't already in flight.
  // This avoids overflowing the system task queue when many Pebble Protocol
  // messages arrive in rapid succession (e.g. after BT reconnect).
  if (impl->should_use_kernel_main) {
    prv_append_to_pending_list(impl, &s_pending_main_head);
    if (!s_main_callback_pending) {
      s_main_callback_pending = true;
      launcher_task_add_callback(prv_default_kernel_receiver_main_cb, NULL);
    }
  } else {
    prv_append_to_pending_list(impl, &s_pending_bg_head);
    if (!s_bg_callback_pending) {
      s_bg_callback_pending = true;
      system_task_add_callback(prv_default_kernel_receiver_bg_cb, NULL);
    }
  }
}

static void prv_default_kernel_receiver_cleanup(Receiver *receiver) {
  DefaultReceiverImpl *impl = (DefaultReceiverImpl *)receiver;
  if (impl->handler_scheduled) {
    return; // the kernel BG/main callback will free the data
  }
  prv_wipe_receiver_data(impl);
  kernel_free(impl);
}

const ReceiverImplementation g_default_kernel_receiver_implementation = {
  .prepare = prv_default_kernel_receiver_prepare,
  .write = prv_default_kernel_receiver_write,
  .finish = prv_default_kernel_receiver_finish,
  .cleanup = prv_default_kernel_receiver_cleanup,
};
