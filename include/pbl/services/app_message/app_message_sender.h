/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/services/app_outbox_service.h>
#include <pbl/services/comm_session/protocol.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/comm_session/session_send_queue.h>

/**
 * @defgroup services_app_message App message
 * @ingroup services
 * @brief Sending of app Pebble Protocol messages queued in the app outbox.
 *
 * Glue between the app outbox service and the session send queue; all state is kept by the app
 * outbox service. Only the app message endpoint is allowed.
 * @{
 */

/**
 * @brief Sender status, extending @c AppOutboxStatus with values in its user range.
 */
typedef enum {
  /** Message sent. */
  AppMessageSenderErrorSuccess = AppOutboxStatusSuccess,
  /** No session to send on, or it disconnected before the message was sent. */
  AppMessageSenderErrorDisconnected = AppOutboxStatusConsumerDoesNotExist,
  /** Message shorter than its header or with an empty payload. */
  AppMessageSenderErrorDataTooShort = AppOutboxStatusUserRangeStart,
  /** Endpoint not allowed for apps. */
  AppMessageSenderErrorEndpointDisallowed,

  /** Number of status values. */
  NumAppMessageSenderError,
} AppMessageSenderError;

_Static_assert((NumAppMessageSenderError - 1) <= AppOutboxStatusUserRangeEnd,
               "AppMessageSenderError value can't be bigger than AppOutboxStatusUserRangeEnd");

/**
 * @brief Send job, stored as the @c consumer_data of the AppOutboxMessage.
 *
 * Always contained within the AppOutboxMessage.
 */
typedef struct {
  /** Send queue job; must be first. */
  SessionSendQueueJob send_queue_job;

  /** Session the message is sent on. */
  CommSession *session;
  /** Pebble Protocol header, sent before the payload. */
  PebbleProtocolHeader header;

  /** Bytes of header and payload already sent. */
  size_t consumed_length;
} AppMessageSendJob;

_Static_assert(offsetof(AppMessageSendJob, send_queue_job) == 0,
               "send_queue_job must be first member, due to the way session_send_queue.c works");

/**
 * @brief Layout of an outbox message's @c data, in app memory.
 *
 * Untrusted: every field is sanitized before use. Must not grow beyond 12 bytes, as apps depend
 * on it.
 */
typedef struct {
  /** Session to send on, or NULL to select it from the UUID of the running app. */
  CommSession *session;

  /** Reserved. */
  uint8_t padding[6];

  /** Pebble Protocol endpoint. */
  uint16_t endpoint_id;
  /** Pebble Protocol payload, not empty. */
  uint8_t payload[];
} AppMessageAppOutboxData;

#if !UNITTEST && __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(AppMessageAppOutboxData) <= 12,
               "Can't grow AppMessageAppOutboxData beyond 12 bytes, can break apps!");
#endif

/** @brief Register with the app outbox service; called once at boot. */
void app_message_sender_init(void);

/** @} */
