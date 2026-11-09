/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/analytics/analytics.h>
#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/session.h>
#include <pbl/services/app_inbox_service.h>
#include <pbl/util/math.h>

#include <applib/app_message/app_message_internal.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>

PBL_LOG_MODULE_DECLARE(service_app_message, CONFIG_SERVICE_APP_MESSAGE_LOG_LEVEL);

extern const struct pbl_pp_receiver_api g_app_message_receiver_implementation;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Receiver that writes App Message PP messages to the app's memory space using
// app_inbox_service. It also forwards a copy of the header to the default system receiver, but
// with a special handler that will always send a nack reply. If all goes well, this forward is
// cancelled in the end and the nack does not get sent.

//! The maximum amount of header bytes that is needed in order to let the system nack it.
//! To nack an App Message push, only the transaction ID is needed. Therefore, only buffer the
//! AppMessageHeader of the incoming push:
#define MAX_HEADER_SIZE (sizeof(AppMessageHeader))

typedef struct {
  bool is_writing_to_app_inbox;

  struct pbl_pp_session *session;

  //! Used to keep track of how many header bytes are remaining to either forward to the default
  //! system receiver or to save them in the event the app inbox write fails in the end.
  //! We only want to write up to MAX_HEADER_SIZE, to keep the kernel heap impact to a minimum.
  size_t header_bytes_remaining;

  //! Pointer to the default system receiver context, to which we want to forward the header data.
  struct pbl_pp_receiver *kernel_receiver;
} AppMessageReceiver;

static bool prv_fwd_prepare(AppMessageReceiver *rcv, struct pbl_pp_session *session,
                            size_t header_bytes_remaining) {
  // Try to set up a forward to the default system receiver that will send a nack back, based
  // on the header of the message:
  static const struct pbl_pp_endpoint kernel_nack_endpoint = {
    .handler = app_message_app_protocol_system_nack_callback,
    .id = APP_MESSAGE_ENDPOINT_ID,
    .access = PBL_PP_ACCESS_ANY,
    .task = PBL_PP_HANDLER_BG,
  };
  struct pbl_pp_receiver *kernel_receiver =
      pbl_pp_kernel_receiver.prepare(session, &kernel_nack_endpoint, header_bytes_remaining);
  if (!kernel_receiver) {
    PBL_LOG_ERR("System receiver wasn't able to prepare");
    return false;
  }
  rcv->kernel_receiver = kernel_receiver;
  return true;
}

static void prv_write(const uint8_t *data, size_t length) {
  app_inbox_service_write(AppInboxServiceTagAppMessageReceiver, data, length);
}

static struct pbl_pp_receiver *prv_app_message_receiver_prepare(
    struct pbl_pp_session *session, const struct pbl_pp_endpoint *endpoint,
    size_t total_payload_size) {
  if (total_payload_size > 500) {
    pbl_pp_session_set_responsiveness(session, PBL_BT_CONSUMER_PP_APP_MESSAGE,
                                      PBL_BT_RESPONSE_TIME_MIN,
                                      PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_MESSAGE_SECS, nullptr);
  }

  AppMessageReceiver *rcv = (AppMessageReceiver *)pbl_zalloc(sizeof(AppMessageReceiver));
  if (!rcv) {
    return nullptr;
  }
  rcv->session = session;

  const size_t header_bytes_remaining = MIN(MAX_HEADER_SIZE, total_payload_size);
  rcv->header_bytes_remaining = header_bytes_remaining;

  // Always forward the header to default system receiver as well, we'll cancel it later on if the
  // message was written successfully to the app inbox.
  if (!prv_fwd_prepare(rcv, session, header_bytes_remaining)) {
    pbl_free(rcv);
    return nullptr;
  }

  const size_t total_size = sizeof(AppMessageReceiverHeader) + total_payload_size;

  // Reasons why app_inbox_service_begin() might fail:
  // - the watchapp does not have App Message context opened
  // - there is no more space in the buffer that the app had allocated for it,
  // - the inbox is already being written to (by another session) -- should be very rare
  if (app_inbox_service_begin(AppInboxServiceTagAppMessageReceiver, total_size, session)) {
    rcv->is_writing_to_app_inbox = true;

    // Log most recent communication timestamp
    const AppInstallId app_id = app_manager_get_current_app_id();
    app_install_mark_prioritized(app_id, true /* can_expire */);

    // Write the header, this info is needed for the app to handle the message and reply:
    const AppMessageReceiverHeader header = (const AppMessageReceiverHeader){
      .session = session,
    };
    prv_write((const uint8_t *)&header, sizeof(header));
  }

  return (struct pbl_pp_receiver *)rcv;
}

static void prv_app_message_receiver_write(struct pbl_pp_receiver *receiver, const uint8_t *data,
                                           size_t length) {
  AppMessageReceiver *rcv = (AppMessageReceiver *)receiver;

  pbl_pp_session_set_responsiveness(rcv->session, PBL_BT_CONSUMER_PP_APP_MESSAGE,
                                    PBL_BT_RESPONSE_TIME_MIN,
                                    PBL_BT_MIN_LATENCY_MODE_TIMEOUT_APP_MESSAGE_SECS, nullptr);

  if (rcv->header_bytes_remaining > 0) {
    const size_t header_bytes_to_write = MIN(rcv->header_bytes_remaining, length);
    pbl_pp_kernel_receiver.write(rcv->kernel_receiver, data, header_bytes_to_write);
    rcv->header_bytes_remaining -= header_bytes_to_write;
  }

  if (rcv->is_writing_to_app_inbox) {
    prv_write(data, length);
  }
}

static void prv_finally(AppMessageReceiver *receiver,
                        void (*kernel_receiver_finally_cb)(struct pbl_pp_receiver *)) {
  kernel_receiver_finally_cb(receiver->kernel_receiver);
  pbl_free(receiver);
}

static void prv_app_message_receiver_finish(struct pbl_pp_receiver *receiver) {
  AppMessageReceiver *rcv = (AppMessageReceiver *)receiver;

  // Default to letting the system receiver process the message and thus nack it:
  void (*kernel_receiver_finally_cb)(struct pbl_pp_receiver *) = pbl_pp_kernel_receiver.finish;

  if (rcv->is_writing_to_app_inbox) {
    if (app_inbox_service_end(AppInboxServiceTagAppMessageReceiver)) {
      // The write was successful, cancel processing the header for nacking:
      kernel_receiver_finally_cb = pbl_pp_kernel_receiver.cleanup;
      PBL_ANALYTICS_ADD(app_message_received_count, 1);
    }
  }

  prv_finally(rcv, kernel_receiver_finally_cb);
}

static void prv_app_message_receiver_cleanup(struct pbl_pp_receiver *receiver) {
  AppMessageReceiver *rcv = (AppMessageReceiver *)receiver;

  if (rcv->is_writing_to_app_inbox) {
    // Cancel the write, we don't want to deliver a broken message to the watchapp:
    app_inbox_service_cancel(AppInboxServiceTagAppMessageReceiver);
  }

  prv_finally(rcv, pbl_pp_kernel_receiver.cleanup);
}

const struct pbl_pp_receiver_api g_app_message_receiver_implementation = {
  .prepare = prv_app_message_receiver_prepare,
  .write = prv_app_message_receiver_write,
  .finish = prv_app_message_receiver_finish,
  .cleanup = prv_app_message_receiver_cleanup,
};

PBL_PP_ENDPOINT_DEFINE_RECEIVER(PBL_PP_ENDPOINT_APP_MESSAGE, PBL_PP_ACCESS_ANY,
                                g_app_message_receiver_implementation);
