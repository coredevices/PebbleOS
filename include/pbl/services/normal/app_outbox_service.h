/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_outbox.h"
#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "util/list.h"

#include <stdint.h>
#include <stddef.h>

// Design goals of this module:
//
// - Provide a generic mechanism to pass variable-length data from app to kernel service.
// - Have the data be read directly from an app-provided buffer (in app space).
// - Asynchronous: a "sent" callback should execute on the (app) task that created the outbox, when
//   the transfer is completed.
// - Simple status results: the "sent" callback should be called with a simple status code that
//   indicates whether the transfer was successful or not.
// - Use is limited only to the hard-coded set of permitted use cases and their handlers, to avoid
//   abuse of the API by misbehaving apps.
// - The kernel manages the existence of service instances. If data is sent while the service is not
//   registered, the sent_handler should be called right away with a failure.
// - Allow adding a message while there is already one or more waiting in the outbox.
//
// Non-goals:
//
// - Ability to cancel messages that have already been added to the outbox (could be added easily
//   in the future)

typedef enum {
  AppOutboxServiceTagInvalid = -1,
  AppOutboxServiceTagAppMessageSender,
#ifdef UNITTEST
  AppOutboxServiceTagUnitTest,
#endif
  NumAppOutboxServiceTag,
} AppOutboxServiceTag;

//! To be called once at boot.
void app_outbox_service_init(void);

//! Cleans up all pending messages. To be called by the app manager when an app is terminated.
//! @note This will *NOT* invoke the `sent_handler`s of the pending messages.
void app_outbox_service_cleanup_all_pending_messages(void);

//! Cleans up any pending app outbox events in the queue towards the kernel, that have not been
//! processed.
void app_outbox_service_cleanup_event(PebbleEvent *event);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sender (App) API

//! @see app_outbox_send for documentation

////////////////////////////////////////////////////////////////////////////////////////////////////
// Owner / Receiver (Kernel) API

typedef struct {
  ListNode node;

  //! Pointer to message data
  //! @note This will reside in app's memory space and never in kernel memory space. Therefore the
  //! contents should be sanity checked carefully.
  const uint8_t *data;

  //! The length of `data` in bytes
  size_t length;

  //! Callback to execute on app task, when the data is consumed by the receiver.
  AppOutboxSentHandler sent_handler;
  //! User context to pass into the `sent_handler` callback.
  void *cb_ctx;

  //! Additional user data that will be allocated by app_outbox_service, on behalf of the receiving
  //! kernel service. This can be used to store any state needed to parse and process the message.
  //! The buffer will be zeroed out just before the message handler gets called.
  uint8_t consumer_data[];
} AppOutboxMessage;

//! Callback to indicate there is a message added.
//! @note Only `consumer_data` is allowed to be mutated by the client!
typedef void (*AppOutboxMessageHandler)(AppOutboxMessage *message);

//! Can be used by the receiving kernel service to check whether message has been cancelled in the
//! mean time. Note that app_outbox_service_consume_message() still MUST be called with a cancelled
//! message at some point in time, to clean up the resources associated with it.
bool app_outbox_service_is_message_cancelled(AppOutboxMessage *message);

//! Registers a consumer for a specific app outbox service tag.
//! @param consumer_data_size The additional space that will be allocated for context by the app
//! outbox service, on behalf of the consumer. The extra space will be appended to the message that
//! gets passed into `message_handler`.
void app_outbox_service_register(AppOutboxServiceTag service_tag,
                                 AppOutboxMessageHandler message_handler,
                                 PebbleTask consumer_task,
                                 size_t consumer_data_size);

//! Will invoke the sender's `sent_handler` with the status on the app task.
//! @param message Pointer to the message to be consumed. Note that this message will have been
//! free'd after this function returns and should not be used thereafter.
void app_outbox_service_consume_message(AppOutboxMessage *message, AppOutboxStatus status);

//! Closes the outbox.
//! This will call the `sent_handler` callback for all messages in the outbox with
//! AppOutboxStatusConsumerDoesNotExist.
void app_outbox_service_unregister(AppOutboxServiceTag service_tag);
