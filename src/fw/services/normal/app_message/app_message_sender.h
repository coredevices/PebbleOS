/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/app_outbox_service.h"
#include "services/common/comm_session/protocol.h"
#include "services/common/comm_session/session.h"
#include "services/common/comm_session/session_send_queue.h"

#include <stdint.h>

//! This module uses AppOutbox to get Pebble Protocol outbound messages from the app.
//! It does not keep any static state inside this module, all the state is stored by the app outbox
//! service. It's really just a piece of glue code between app_outbox.c and session_send_queue.c


//! Enum that "inherits" from AppOutboxStatus and defines app-message-sender-specific status
//! values in the user range:
typedef enum {
  AppMessageSenderErrorSuccess = AppOutboxStatusSuccess,
  AppMessageSenderErrorDisconnected = AppOutboxStatusConsumerDoesNotExist,
  AppMessageSenderErrorDataTooShort = AppOutboxStatusUserRangeStart,
  AppMessageSenderErrorEndpointDisallowed,

  NumAppMessageSenderError,
} AppMessageSenderError;

_Static_assert((NumAppMessageSenderError - 1) <= AppOutboxStatusUserRangeEnd,
               "AppMessageSenderError value can't be bigger than AppOutboxStatusUserRangeEnd");

//! @note This is the data structure for the `consumer_data` of the AppOutboxMessage.
//! app_message_sender.c assumes this struct is always contained within the AppOutboxMessage
//! struct.
typedef struct {
  SessionSendQueueJob send_queue_job;

  CommSession *session;
  PebbleProtocolHeader header;

  size_t consumed_length;
} AppMessageSendJob;

_Static_assert(offsetof(AppMessageSendJob, send_queue_job) == 0,
               "send_queue_job must be first member, due to the way session_send_queue.c works");

//! Structure of `data` in outbox_message (in app's memory space)
//! @note None of these fields can be trusted / used as is, they need to be sanitized.
typedef struct {
  //! Can be NULL to "auto select" the session based on the UUID of the running app.
  CommSession *session;

  //! Padding for future use
  uint8_t padding[6];

  uint16_t endpoint_id;
  uint8_t payload[];
} AppMessageAppOutboxData;

#if !UNITTEST
_Static_assert(sizeof(AppMessageAppOutboxData) <= 12,
               "Can't grow AppMessageAppOutboxData beyond 12 bytes, can break apps!");
#endif

//! To be called once during boot. This registers this module with app_outbox_service.
void app_message_sender_init(void);
