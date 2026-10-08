/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/util/list.h>

#include <applib/app_outbox.h>
#include <kernel/events.h>
#include <kernel/pebble_tasks.h>

/**
 * @defgroup services_app_outbox_service App outbox service
 * @ingroup services
 * @brief Passes variable-length messages from an app buffer to a kernel service.
 *
 * Apps send with app_outbox_send(). The data is read directly from the app's buffer and the
 * transfer is asynchronous: once the receiving kernel service consumes the message, the sender's
 * sent handler runs on the app task with a simple status. Only a hard-coded set of sent handlers
 * is permitted, each mapping to one service tag, to prevent abuse by misbehaving apps. If no
 * consumer is registered for the tag, the sent handler is called right away with a failure.
 * Several messages may be pending at once. Cancelling a message already added is not supported.
 * @{
 */

/** @brief Identifies a kernel consumer and the sent handler permitted for it. */
typedef enum {
  /** Invalid tag. */
  AppOutboxServiceTagInvalid = -1,
  /** App message sender. */
  AppOutboxServiceTagAppMessageSender,
#ifdef UNITTEST
  /** Unit tests. */
  AppOutboxServiceTagUnitTest,
#endif
  /** Number of tags. */
  NumAppOutboxServiceTag,
} AppOutboxServiceTag;

/** @brief Initialize the service, once at boot. */
void app_outbox_service_init(void);

/**
 * @brief Drop all pending messages.
 *
 * Called by the app manager when an app terminates. The sent handlers are not called; the messages
 * are freed once their consumers call app_outbox_service_consume_message().
 */
void app_outbox_service_cleanup_all_pending_messages(void);

/**
 * @brief Free the message of an unprocessed app outbox event.
 *
 * Used when cleaning up events queued towards the kernel. Other event types are ignored.
 *
 * @param event Event to clean up.
 */
void app_outbox_service_cleanup_event(PebbleEvent *event);

/** @brief Message sent by an app, as seen by the consuming kernel service. */
typedef struct {
  /** List node, for internal use. */
  ListNode node;

  /**
   * Message data. It resides in app memory, so its contents must be checked carefully.
   */
  const uint8_t *data;

  /** Length of @ref data in bytes. */
  size_t length;

  /** Called on the app task when the message is consumed. */
  AppOutboxSentHandler sent_handler;
  /** Context passed to @ref sent_handler. */
  void *cb_ctx;

  /**
   * Zero-initialized space of the size given to app_outbox_service_register(), for the consumer
   * to keep the state it needs to process the message.
   */
  uint8_t consumer_data[];
} AppOutboxMessage;

/**
 * @brief Called on the consumer task when a message is added.
 *
 * Only @ref AppOutboxMessage::consumer_data may be modified by the handler.
 *
 * @param message Added message.
 */
typedef void (*AppOutboxMessageHandler)(AppOutboxMessage *message);

/**
 * @brief Check whether a message was cancelled.
 *
 * A message is cancelled when its consumer unregisters or the app terminates.
 * app_outbox_service_consume_message() must still be called on a cancelled message to free it.
 *
 * @param message Message to check.
 * @return true if the message was cancelled.
 */
bool app_outbox_service_is_message_cancelled(AppOutboxMessage *message);

/**
 * @brief Register the consumer of a tag.
 *
 * Only one consumer per tag is allowed.
 *
 * @param service_tag Tag to consume.
 * @param message_handler Called when a message is added.
 * @param consumer_task Task on which @p message_handler runs.
 * @param consumer_data_size Bytes allocated in each message for
 *                           @ref AppOutboxMessage::consumer_data.
 */
void app_outbox_service_register(AppOutboxServiceTag service_tag,
                                 AppOutboxMessageHandler message_handler, PebbleTask consumer_task,
                                 size_t consumer_data_size);

/**
 * @brief Finish processing a message and free it.
 *
 * Schedules the sender's sent handler on the app task, unless the message was cancelled.
 *
 * @param message Message to consume; invalid once this returns.
 * @param status Status reported to the sent handler.
 */
void app_outbox_service_consume_message(AppOutboxMessage *message, AppOutboxStatus status);

/**
 * @brief Unregister the consumer of a tag.
 *
 * The sent handlers of all pending messages are called with
 * @c AppOutboxStatusConsumerDoesNotExist.
 *
 * @param service_tag Tag to unregister.
 */
void app_outbox_service_unregister(AppOutboxServiceTag service_tag);

/** @} */
