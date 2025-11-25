/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_message/app_message_internal.h"
#include "applib/app_outbox.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "services/common/analytics/analytics.h"
#include "services/normal/app_message/app_message_sender.h"
#include "system/logging.h"
#include "util/math.h"
#include "util/net.h"

// -------------------------------------------------------------------------------------------------
// Misc helpers:

static void prv_request_fast_connection(CommSession *session) {
  // TODO: apply some heuristic to decide whether to put connection in fast mode or not:
  // https://pebbletechnology.atlassian.net/browse/PBL-21538
  comm_session_set_responsiveness(session, BtConsumerPpAppMessage, ResponseTimeMin,
                                  MIN_LATENCY_MODE_TIMEOUT_APP_MESSAGE_SECS);
}

static AppOutboxMessage *prv_outbox_message_from_app_message_send_job(
      AppMessageSendJob *app_message_send_job) {
  const size_t offset = offsetof(AppOutboxMessage, consumer_data);
  return (AppOutboxMessage *)(((uint8_t *)app_message_send_job) - offset);
}

static AppOutboxMessage *prv_outbox_message_from_send_job(SessionSendQueueJob *send_job) {
  return prv_outbox_message_from_app_message_send_job((AppMessageSendJob *)send_job);
}


// -------------------------------------------------------------------------------------------------
// Interfaces towards Send Queue:

static size_t prv_get_length(AppMessageSendJob *app_message_send_job) {
  AppOutboxMessage *outbox_message =
      prv_outbox_message_from_app_message_send_job(app_message_send_job);
  return (outbox_message->length - offsetof(AppMessageAppOutboxData, payload) +
          sizeof(PebbleProtocolHeader) - app_message_send_job->consumed_length);
}

static bool prv_is_header_consumed_for_offset(uint32_t offset) {
  return (offset >= sizeof(PebbleProtocolHeader));
}

static size_t prv_get_read_pointer(AppMessageSendJob *app_message_send_job,
                                   uint32_t offset, const uint8_t **data_out) {
  const uint8_t *read_pointer;
  size_t num_bytes_available;
  if (prv_is_header_consumed_for_offset(offset)) {
    AppOutboxMessage *outbox_message =
        prv_outbox_message_from_app_message_send_job(app_message_send_job);

    // Avoid reading from the buffer in app space if the message was cancelled,
    // just read zeroes instead.
    // Note: we could consider removing messages from the send queue that have not been started to
    // get sent out at all. This requires the send queue to keep track of what has started and
    // what not, and requires transports to tell the send queue what it has in flight so far.
    const bool is_cancelled = app_outbox_service_is_message_cancelled(outbox_message);
    if (is_cancelled) {
      static const uint32_t s_zeroes = 0;
      *data_out = (const uint8_t *)&s_zeroes;
      return sizeof(s_zeroes);
    }

    const AppMessageAppOutboxData *outbox_data =
        (const AppMessageAppOutboxData *)outbox_message->data;
    read_pointer = (outbox_data->payload - sizeof(PebbleProtocolHeader));
    num_bytes_available = prv_get_length(app_message_send_job);
  } else {
    read_pointer = (const uint8_t *)&app_message_send_job->header;
    num_bytes_available = (sizeof(PebbleProtocolHeader) - offset);
  }
  read_pointer += offset;
  *data_out = read_pointer;

  return num_bytes_available;
}

static size_t prv_send_job_impl_get_length(const SessionSendQueueJob *send_job) {
  return prv_get_length((AppMessageSendJob *)send_job);
}

static size_t prv_send_job_impl_copy(const SessionSendQueueJob *send_job, int start_offset,
                              size_t length, uint8_t *data_out) {
  AppMessageSendJob *app_message_send_job = (AppMessageSendJob *)send_job;
  prv_request_fast_connection(app_message_send_job->session);

  const size_t length_available = prv_get_length(app_message_send_job);
  const size_t length_after_offset = (length_available - start_offset);
  const size_t length_to_copy = MIN(length_after_offset, length);

  size_t length_remaining = length_to_copy;
  while (length_remaining) {
    const uint8_t *part_data;
    uint32_t data_out_pos = (length_to_copy - length_remaining);
    size_t part_length =
        prv_get_read_pointer(app_message_send_job,
                             app_message_send_job->consumed_length + start_offset + data_out_pos,
                             &part_data);
    part_length = MIN(part_length, length_remaining);
    memcpy(data_out + data_out_pos, part_data, part_length);
    length_remaining -= part_length;
  }

  return length_to_copy;
}

static size_t prv_send_job_impl_get_read_pointer(const SessionSendQueueJob *send_job,
                                                 const uint8_t **data_out) {
  AppMessageSendJob *app_message_send_job = (AppMessageSendJob *)send_job;
  prv_request_fast_connection(app_message_send_job->session);

  return prv_get_read_pointer(app_message_send_job,
                              app_message_send_job->consumed_length, data_out);
}

static void prv_send_job_impl_consume(const SessionSendQueueJob *send_job, size_t length) {
  AppMessageSendJob *app_message_send_job = (AppMessageSendJob *)send_job;
  app_message_send_job->consumed_length += length;

  analytics_add(ANALYTICS_APP_METRIC_MSG_BYTE_OUT_COUNT, length, AnalyticsClient_App);
}

static void prv_send_job_impl_free(SessionSendQueueJob *send_job) {
  AppMessageSendJob *app_message_send_job = (AppMessageSendJob *)send_job;
  AppOutboxMessage *outbox_message = prv_outbox_message_from_send_job(send_job);
  const bool is_completed = (0 == prv_get_length(app_message_send_job));
  if (is_completed) {
    const AppInstallId app_id = app_manager_get_current_app_id();
    app_install_mark_prioritized(app_id, true /* can_expire */);

    analytics_inc(ANALYTICS_APP_METRIC_MSG_OUT_COUNT, AnalyticsClient_App);
  }
  // The outbox_message is owned by app_outbox_service, calling consume will free it as well:
  const AppOutboxStatus status =
      (const AppOutboxStatus) (is_completed ? AppMessageSenderErrorSuccess :
                                              AppMessageSenderErrorDisconnected);
  app_outbox_service_consume_message(outbox_message, status);
}

T_STATIC const SessionSendJobImpl s_app_message_send_job_impl = {
  .get_length = prv_send_job_impl_get_length,
  .copy = prv_send_job_impl_copy,
  .get_read_pointer = prv_send_job_impl_get_read_pointer,
  .consume = prv_send_job_impl_consume,
  .free = prv_send_job_impl_free,
};


// -------------------------------------------------------------------------------------------------
// Interfaces towards App Outbox service:

static bool prv_is_endpoint_allowed(uint16_t endpoint_id) {
  return (endpoint_id == APP_MESSAGE_ENDPOINT_ID);
}

static AppMessageSenderError prv_sanity_check_msg_and_fill_header(const AppOutboxMessage *message) {
  if (message->length < (sizeof(AppMessageAppOutboxData) + 1 /* Prohibit zero length PP msg */)) {
    return AppMessageSenderErrorDataTooShort;
  }

  const AppMessageAppOutboxData *outbox_data = (const AppMessageAppOutboxData *)message->data;

  const uint16_t endpoint_id = outbox_data->endpoint_id;
  if (!prv_is_endpoint_allowed(endpoint_id)) {
    return AppMessageSenderErrorEndpointDisallowed;
  }

  const size_t pp_payload_length = (message->length - offsetof(AppMessageAppOutboxData, payload));
  AppMessageSendJob *app_message_send_job = (AppMessageSendJob *)message->consumer_data;
  app_message_send_job->header = (const PebbleProtocolHeader) {
    .endpoint_id = htons(endpoint_id),
    .length = htons(pp_payload_length),
  };

  return AppMessageSenderErrorSuccess;
}

static void prv_handle_outbox_message(AppOutboxMessage *message) {
  AppMessageSendJob *app_message_send_job = (AppMessageSendJob *)message->consumer_data;
  *app_message_send_job = (const AppMessageSendJob) {
    .send_queue_job = {
      .impl = &s_app_message_send_job_impl,
    },
    .consumed_length = 0,
  };

  const AppMessageSenderError err = prv_sanity_check_msg_and_fill_header(message);
  if (AppMessageSenderErrorSuccess != err) {
    PBL_LOG(LOG_LEVEL_ERROR, "Outbound app message corrupted %u", err);
    app_outbox_service_consume_message(message, (AppOutboxStatus)err);
    return;
  }

  const AppMessageAppOutboxData *outbox_data =
      (const AppMessageAppOutboxData *)message->data;

  app_message_send_job->session = outbox_data->session;
  comm_session_sanitize_app_session(&app_message_send_job->session);
  if (!app_message_send_job->session) {
    // Most likely disconnected in the mean time, don't spam our logs about this
    app_outbox_service_consume_message(message, (AppOutboxStatus)AppMessageSenderErrorDisconnected);
    return;
  }

  prv_request_fast_connection(app_message_send_job->session);
  comm_session_send_queue_add_job(app_message_send_job->session,
                                  (SessionSendQueueJob **)&app_message_send_job);
}

// -------------------------------------------------------------------------------------------------

void app_message_sender_init(void) {
  const size_t consumer_data_size = sizeof(AppMessageSendJob);
  // Make prv_handle_outbox_message() execute on KernelMain:
  app_outbox_service_register(AppOutboxServiceTagAppMessageSender,
                              prv_handle_outbox_message, PebbleTask_KernelMain, consumer_data_size);
}
