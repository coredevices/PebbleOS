/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/**
 * @brief Remember the title of the notification the Chat app is being opened from.
 *
 * @param title Notification title, copied; NULL forgets it.
 */
void chat_service_set_launch_hint(const char *title);

/**
 * @brief Take the title set by chat_service_set_launch_hint(), clearing it.
 *
 * @param[out] out Destination, NUL terminated.
 * @param out_size Size of @p out.
 * @return true if there was one.
 */
bool chat_service_take_launch_hint(char *out, size_t out_size);

/** @} */
