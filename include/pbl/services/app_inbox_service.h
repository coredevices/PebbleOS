/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

#include <applib/app_inbox.h>

/**
 * @defgroup services_app_inbox_service App inbox service
 * @ingroup services
 * @brief Passes variable-length messages from a kernel service into an app-provided buffer.
 *
 * Design goals:
 * - Data is written directly into a buffer in app space, as contiguous messages (no wrap-around)
 *   so they are easy to parse.
 * - A message can be written while earlier messages are still pending in the buffer.
 * - A partially written message can be cancelled and is then never delivered.
 * - No race can make the app read an incomplete message.
 * - The app is told how many messages were dropped for lack of space.
 *
 * Non-goals: sharing a buffer between kernel services (one buffer per service tag), concurrent
 * writers (a write fails up front while another task is writing), and preserving the order of
 * dropped messages relative to received ones.
 *
 * The kernel writes a message with app_inbox_service_begin(), any number of
 * app_inbox_service_write() calls and app_inbox_service_end(). A callback event is then posted to
 * the registering task, where the message and dropped handlers run.
 *
 * @code{.c}
 * if (app_inbox_service_begin(AppInboxServiceTagAppMessageReceiver, len, writer)) {
 *   app_inbox_service_write(AppInboxServiceTagAppMessageReceiver, data, len);
 *   app_inbox_service_end(AppInboxServiceTagAppMessageReceiver);
 * }
 * @endcode
 * @{
 */

/** @brief Identifies an inbox and the permitted message and dropped handlers for it. */
typedef enum {
  /** Invalid tag. */
  AppInboxServiceTagInvalid = -1,
  /** App message receiver. */
  AppInboxServiceTagAppMessageReceiver,
#ifdef UNITTEST
  /** Unit tests. */
  AppInboxServiceTagUnitTest,
  /** Unit tests, alternate handlers. */
  AppInboxServiceTagUnitTestAlt,
#endif
  /** Number of tags. */
  NumAppInboxServiceTag,
} AppInboxServiceTag;

/** @brief Header preceding each message in the inbox buffer. */
typedef struct PBL_PACKED {
  /** Length of @ref data, excluding this header. */
  size_t length;
  /**
   * Reserved. The header lives in a buffer sized by the app, so it cannot grow once shipped.
   */
  uint8_t padding[4];
  /** Message payload. */
  uint8_t data[];
} AppInboxMessageHeader;

#if !UNITTEST && __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(AppInboxMessageHeader) == 8,
               "The size of AppInboxMessageHeader cannot grow beyond 8 bytes!");
#endif

/** @brief Initialize the service, once at boot. */
void app_inbox_service_init(void);

/**
 * @brief Register an inbox.
 *
 * The handlers run on the calling task. Fails if an inbox already exists for @p tag or
 * @p storage. Apps call this through app_inbox_create_and_register().
 *
 * @param storage Buffer in app space that receives the messages.
 * @param storage_size Size of @p storage in bytes. Each message also takes
 *                     sizeof(AppInboxMessageHeader) bytes.
 * @param message_handler Called for each received message.
 * @param dropped_handler Called when one or more messages were dropped.
 * @param tag Tag identifying the inbox.
 * @return true on success, false on error or out of memory.
 */
bool app_inbox_service_register(uint8_t *storage, size_t storage_size,
                                AppInboxMessageHandler message_handler,
                                AppInboxDroppedHandler dropped_handler, AppInboxServiceTag tag);

/**
 * @brief Unregister the inbox using @p storage.
 *
 * Apps call this through app_inbox_destroy_and_deregister().
 *
 * @param storage Buffer given to app_inbox_service_register().
 * @return Number of messages dropped or still waiting to be consumed, including one being written.
 */
uint32_t app_inbox_service_unregister_by_storage(uint8_t *storage);

/** @brief Unregister all inboxes. */
void app_inbox_service_unregister_all(void);

/**
 * @brief Start writing a message.
 *
 * If this returns true, app_inbox_service_end() or app_inbox_service_cancel() must be called
 * eventually. If it returns false, none of app_inbox_service_write(), app_inbox_service_end() and
 * app_inbox_service_cancel() may be called; the message counts as dropped.
 *
 * @param tag Inbox to write to.
 * @param required_free_length Length of the message payload, excluding the header. The buffer
 *                             needs this plus sizeof(AppInboxMessageHeader) bytes free.
 * @param writer Non-NULL reference to the writer, for debugging.
 * @return true if the inbox was claimed, false if it does not exist, is being written by another
 *         writer or has not enough space.
 */
bool app_inbox_service_begin(AppInboxServiceTag tag, size_t required_free_length, void *writer);

/**
 * @brief Append data to the message being written.
 *
 * After a failed write, further writes fail too and app_inbox_service_end() reports the message
 * as dropped instead of delivering it.
 *
 * @param tag Inbox being written.
 * @param data Data to append.
 * @param length Length of @p data in bytes.
 * @return true if the data was written.
 */
bool app_inbox_service_write(AppInboxServiceTag tag, const uint8_t *data, size_t length);

/**
 * @brief Finish the message being written and notify the app.
 *
 * @param tag Inbox being written.
 * @return true if the whole message was written and delivered, false if it was dropped, in which
 *         case the dropped handler is called.
 */
bool app_inbox_service_end(AppInboxServiceTag tag);

/**
 * @brief Abort the message being written without delivering it.
 *
 * @param tag Inbox being written.
 */
void app_inbox_service_cancel(AppInboxServiceTag tag);

/** @} */
