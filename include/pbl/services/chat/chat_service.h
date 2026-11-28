/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/services/blob_db/chat_db.h>

/**
 * @defgroup services_chat Chat service
 * @ingroup services
 * @brief Link between the Chat app and the phone app, on endpoint ::PBL_PP_ENDPOINT_CHAT.
 *
 * Both directions carry a version byte (::CHAT_SERVICE_VERSION) and an AppMessage dictionary in
 * the Chat app's message keys. Chats themselves come through ::BlobDBIdChat.
 * @{
 */

/** @brief Version byte that starts every chat message. */
#define CHAT_SERVICE_VERSION 1

/**
 * @brief Receives a dictionary from the phone, on the app task.
 *
 * @param dict Serialized dictionary, valid only during the call.
 * @param len Length of @p dict in bytes.
 */
typedef void (*ChatServiceHandler)(const uint8_t *dict, size_t len);

/**
 * @brief Set the receiver of the phone's dictionaries; NULL drops them.
 *
 * Called by the Chat app from the app task when it starts and exits.
 *
 * @param handler Receiver.
 */
void chat_service_set_handler(ChatServiceHandler handler);

/**
 * @brief Send a dictionary to the phone.
 *
 * @param dict Serialized dictionary.
 * @param len Length of @p dict in bytes.
 * @return true if queued; false if the phone app isn't connected or @p dict is too big.
 */
bool chat_service_send(const uint8_t *dict, size_t len);

/** @brief Where a notification's "Open chat" leads: the chat the Chat app opens. */
typedef struct {
  /** The chat's key in hex, from the phone (exact); "" when it gave none (iOS notifications). */
  char chat_id[CHAT_DB_CHAT_KEY_LEN * 2 + 1];
  /** Without one, the notification, for the app to find the chat holding its message. */
  char title[64];
  char body[256];  // ANCS messages are up to 200 bytes
  /** When it was posted (UTC), 0 if unknown. */
  uint32_t time;
  /** Text for the chat's message bar, sent with its arrow (Index's message to someone not in
   * the favourites), "" for none. */
  char draft[256];
} ChatLaunchHint;

/**
 * @brief Remember the notification the Chat app is being opened from.
 *
 * @param hint Copied. The app opens a chat only when it is surely the notification's, so a reply
 * never goes to another one; otherwise it shows the list.
 */
void chat_service_set_launch_hint(const ChatLaunchHint *hint);

typedef void (*ChatServiceHintHandler)(void);

/**
 * @brief Called on the app task when a launch hint is set while the Chat app is already running
 * (opening it again doesn't restart it, so it wouldn't read the hint). nullptr to stop.
 */
void chat_service_set_hint_handler(ChatServiceHintHandler handler);

/**
 * @brief Take the hint set by chat_service_set_launch_hint(), clearing it.
 *
 * @param[out] out The hint.
 * @return true if there was one.
 */
bool chat_service_take_launch_hint(ChatLaunchHint *out);

/**
 * @brief Ask the phone to archive the newest chat with this title.
 *
 * For a notification's Archive button: notifications name their chat only by title.
 *
 * @param title Chat title.
 * @return true if a chat matched and the request was queued.
 */
bool chat_service_archive_chat_titled(const char *title);

/** @} */
