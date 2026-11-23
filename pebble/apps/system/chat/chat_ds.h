/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
//! Chat's data source. The UI (chat_ui.c, Pebblegram's) speaks the Chat watchapp's AppMessage
//! protocol; this answers it. Chat lists and messages come from the chat db the phone keeps
//! fresh (::BlobDBIdChat), built into the same dictionaries the watchapp's phone side sent.
//! Everything that needs the phone (sending, editing, older history) goes to it as the same
//! dictionaries over the chat service, and its answers come back into the UI's inbox.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <applib/app_message/app_message.h>
#include <applib/dict.h>

void chat_ds_init(void);
void chat_ds_deinit(void);

//! The chat the app was opened for, from a notification's "Open chat"; the UI opens it once the
//! chat list is in. Implemented by chat_ui.c.
void chat_ui_open_chat(const char *chat_id);

//! Whether the app was opened for a chat; the UI then keeps the chat list hidden until it opens.
bool chat_ds_has_launch_hint(void);

// True while the phone is connected to take the app's commands.
bool chat_ds_phone_connected(void);

//! The chat picture size the UI draws (square, in pixels), or 0 when pictures are off.
//! Implemented by chat_ui.c.
int chat_ui_avatar_size(void);

//! Whether the UI already holds chat_id's picture. Implemented by chat_ui.c.
bool chat_ui_has_avatar(const char *chat_id);

AppMessageResult chat_am_open(uint32_t size_inbound, uint32_t size_outbound);
AppMessageResult chat_am_outbox_begin(DictionaryIterator **iterator);
AppMessageResult chat_am_outbox_send(void);
AppMessageInboxReceived chat_am_register_inbox_received(AppMessageInboxReceived cb);
AppMessageInboxDropped chat_am_register_inbox_dropped(AppMessageInboxDropped cb);
AppMessageOutboxSent chat_am_register_outbox_sent(AppMessageOutboxSent cb);
AppMessageOutboxFailed chat_am_register_outbox_failed(AppMessageOutboxFailed cb);

#define app_message_open(in, out)                  chat_am_open((in), (out))
#define app_message_outbox_begin(iter)             chat_am_outbox_begin(iter)
#define app_message_outbox_send()                  chat_am_outbox_send()
#define app_message_register_inbox_received(cb)    chat_am_register_inbox_received(cb)
#define app_message_register_inbox_dropped(cb)     chat_am_register_inbox_dropped(cb)
#define app_message_register_outbox_sent(cb)       chat_am_register_outbox_sent(cb)
#define app_message_register_outbox_failed(cb)     chat_am_register_outbox_failed(cb)
