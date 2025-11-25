/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
  AppOutboxStatusSuccess,
  AppOutboxStatusConsumerDoesNotExist,
  AppOutboxStatusOutOfResources,
  AppOutboxStatusOutOfMemory,

  // Clients of app outbox can use this range for use-case specific status codes:
  AppOutboxStatusUserRangeStart,
  AppOutboxStatusUserRangeEnd = 0xff,
} AppOutboxStatus;

typedef void (*AppOutboxSentHandler)(AppOutboxStatus status, void *cb_ctx);

//! Sends a message to the outbox.
//! If the outbox is not registered, the `sent_handler` will be called immediately with
//! status AppOutboxStatusConsumerDoesNotExist, *after* this function returns.
//! @param data The message data. The caller is responsible for keeping this buffer around until
//! `sent_handler` is called. Don't write to this buffer after calling this either, or the sent
//! data might get corrupted.
//! @param length Size of `data` in number of bytes.
//! @param sent_handler Pointer to the function to call when the kernel has consumed the message,
//! after which the `data` buffer will be no longer in use. Note that the `sent_handler` MUST be
//! white-listed in app_outbox_service.c.
//! @param cb_ctx Pointer to user data that will be passed into the `sent_handler`
void app_outbox_send(const uint8_t *data, size_t length,
                     AppOutboxSentHandler sent_handler, void *cb_ctx);

//! To be called once per app launch by the system.
void app_outbox_init(void);
