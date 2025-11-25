/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct AppInboxConsumerInfo AppInboxConsumerInfo;

//! @param data Pointer to the available data.
//! @param length The length of the available data.
//! @param consumer_info Opaque context object to be passed into app_inbox_consume(). It is NOT
//! mandatory for the handler to call app_inbox_consume().
typedef void (*AppInboxMessageHandler)(const uint8_t *data, size_t length,
                                       AppInboxConsumerInfo *consumer_info);

typedef void (*AppInboxDroppedHandler)(uint32_t num_dropped_messages);

//! Opaque reference to an app inbox.
typedef struct AppInbox AppInbox;

//! @param min_num_messages The minimum number of messages that the inbox should be able to hold
//! if the total payload size is exactly buffer_size. This is used to calculate how much additional
//! buffer space has to be allocated for message header overhead.
//! @param message_handler The callback that will handle received messages (required).
//! @param dropped_handler The callback that will handle dropped messages (optional, use NULL if
//! not needed).
//! @note The system only allows certain handlers, see prv_tag_for_event_handlers()
//! in app_inbox_service.c.
//! @return An opaque value that can be used with app_inbox_destroy_and_deregister, or NULL
//! if the process failed.
AppInbox *app_inbox_create_and_register(size_t buffer_size, uint32_t min_num_messages,
                                        AppInboxMessageHandler message_handler,
                                        AppInboxDroppedHandler dropped_handler);

//! @param app_inbox_ref The app inbox to destroy, pass in the value that
//! app_inbox_create_and_register returned.
//! @return The number of messages that were dropped, plus the ones that were still waiting
//! to be consumed.
uint32_t app_inbox_destroy_and_deregister(AppInbox *app_inbox_ref);

//! Call this function from a AppInboxMessageHandler to immediately consume the message and free
//! up the space in the buffer that was occupied by the message.
//! @param consume_info The opaque context object as passed into the AppInboxMessageHandler.
void app_inbox_consume(AppInboxConsumerInfo *consume_info);
