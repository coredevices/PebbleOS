/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "applib/app_inbox.h"
#include "util/attributes.h"

// Design goals of this module:
//
// - Provide a generic mechanism to pass variable-length data from a kernel service to app.
// - Have the data be written directly into an app-provided buffer (in app space).
// - Data is chunked up in "messages".
// - Data must be contiguously stored for easy parsing (no circular buffer wrap-arounds).
// - Support writing a message, while having pending, unconsumed message(s) in the buffer.
// - Support starting to write a partial message, write some more and finally decide to cancel it.
//   The partial message should not get delivered.
// - No race conditions can exist that could cause reading of an incomplete message.
// - Support for notifying the app when data has been dropped (not enough buffer space) and
//   report the number of dropped messages.
//
// Non-goals:
// - Sharing the same buffer between multiple kernel services (1:1 service to buffer relation is OK)
// - Concurrently writing to the inbox from multiple tasks (failing the write up front when another
//   task is currently in the process of writing a message is OK)
// - Preserve the ordering of when the dropped messages happened vs the received messages (it's OK
//   to only report the number of dropped messages)

typedef enum {
  AppInboxServiceTagInvalid = -1,
  AppInboxServiceTagAppMessageReceiver,
#ifdef UNITTEST
  AppInboxServiceTagUnitTest,
  AppInboxServiceTagUnitTestAlt,
#endif
  NumAppInboxServiceTag,
} AppInboxServiceTag;

typedef struct PACKED {
  // Length of `data` payload (excluding the size of this header)
  size_t length;
  //! To give us some room for future changes. This structure ends up in a buffer that is sized by
  //! the app, so we can't easily increase the size of this once shipped.
  uint8_t padding[4];
  uint8_t data[];
} AppInboxMessageHeader;

#ifndef UNITTEST
_Static_assert(sizeof(AppInboxMessageHeader) == 8,
               "The size of AppInboxMessageHeader cannot grow beyond 8 bytes!");
#endif

//! To be called once at boot.
void app_inbox_service_init(void);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Owner / Receiver (App) API

//! @param storage_size The size of the buffer (in app space). Note that a header will be appended
//! to the data of sizeof(AppInboxMessageHeader) bytes.
//! @note The event handler will be executed on the task that called this function.
//! @see app_inbox_create_and_register() for the applib invocation.
bool app_inbox_service_register(uint8_t *storage, size_t storage_size,
                                AppInboxMessageHandler message_handler,
                                AppInboxDroppedHandler dropped_handler, AppInboxServiceTag tag);

//! @return The number of messages that were dropped, plus the ones that were still waiting
//! to be consumed.
//! @see app_inbox_destroy_and_deregister() for the applib invocation.
uint32_t app_inbox_service_unregister_by_storage(uint8_t *storage);

void app_inbox_service_unregister_all(void);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Sender (Kernel) API

//! @param required_free_length The length in bytes of the data that needs to be written. Note that
//! this should not include the size of the AppInboxMessageHeader. However, there must be at least
//! (required_free_length + sizeof(AppInboxMessageHeader)) bytes free in the buffer in order to
//! be able to write the message.
//! @param writer Reference to the writer, just for debugging.
//! @return True if the buffer is claimed successfully, false if not. If this function returns
//! true, you MUST call app_inbox_service_end() at some point. Inversely, if this functions returns
//! false, you MUST NOT call app_inbox_service_write() nor app_inbox_service_end() nor
//! app_inbox_service_cancel().
bool app_inbox_service_begin(AppInboxServiceTag tag, size_t required_free_length, void *writer);

//! @return True if the write was successful, false if not. If one write failed, successive writes
//! will also fail and `app_inbox_service_end` will not actually dispatch the (broken) message,
//! but instead just dispatch an event that data got dropped.
bool app_inbox_service_write(AppInboxServiceTag tag, const uint8_t *data, size_t length);

//! @return True is the entire message was written successfully, false if not. If a partial write
//! failed, the "dropped handler" will be invoked.
bool app_inbox_service_end(AppInboxServiceTag tag);

void app_inbox_service_cancel(AppInboxServiceTag tag);
