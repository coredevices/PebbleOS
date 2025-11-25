/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "session_receive_router.h"

#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "services/common/comm_session/session.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "system/passert.h"

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
  CommSession *session;
  const PebbleProtocolEndpoint *endpoint;
  size_t total_payload_size;
  int curr_pos;
  bool handler_scheduled;
  bool should_use_kernel_main;
  uint8_t payload[];
} DefaultReceiverImpl;

static Receiver *prv_default_kernel_receiver_prepare(
    CommSession *session, const PebbleProtocolEndpoint *endpoint,
    size_t total_payload_size) {
  if (total_payload_size == 0) {
    return NULL;  // Ignore zero-length messages
  }

  size_t size_needed = sizeof(DefaultReceiverImpl) + total_payload_size;
  DefaultReceiverImpl *receiver = kernel_zalloc(size_needed);

  if (!receiver) {
    PBL_LOG(LOG_LEVEL_WARNING, "Could not allocate receiver, handler:%p size:%d",
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

static void prv_default_kernel_receiver_cb(void *data) {
  DefaultReceiverImpl *impl = (DefaultReceiverImpl *)data;
  PBL_ASSERTN(impl && impl->handler_scheduled && impl->session);

  impl->endpoint->handler(impl->session, impl->payload, impl->total_payload_size);

  prv_wipe_receiver_data(impl);
  kernel_free(impl);
}

static void prv_default_kernel_receiver_finish(Receiver *receiver) {
  DefaultReceiverImpl *impl = (DefaultReceiverImpl *)receiver;
  impl->handler_scheduled = true;

  if ((int)impl->total_payload_size != impl->curr_pos) {
    PBL_LOG(LOG_LEVEL_WARNING, "Got fewer bytes than expected for handler %p",
            impl->endpoint->handler);
  }

  // Note: At the moment we unconditionally spawn a new callback upon
  // completion of each individual payload. If we are getting a flood of
  // events, this may generate too many CBs and overflow the queue. We could keep a list
  // of pending receiver events and only schedule the CB if there isn't one
  // already pending
  if (impl->should_use_kernel_main) {
    launcher_task_add_callback(prv_default_kernel_receiver_cb, receiver);
  } else {
    system_task_add_callback(prv_default_kernel_receiver_cb, receiver);
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
