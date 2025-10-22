/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applib/app_message/app_message_internal.h"
#include "applib/app_message/app_message_receiver.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"
#include "syscall/syscall.h"

static uint32_t s_app_message_received_count = 0;

AppMessageResult app_message_inbox_open(AppMessageCtxInbox *inbox, size_t size_inbound) {
  const size_t size_maximum = app_message_inbox_size_maximum();
  if (size_inbound > size_maximum) {
    // Truncate if it's more than the max:
    size_inbound = size_maximum;
  } else if (size_inbound == size_maximum) {
    APP_LOG(LOG_LEVEL_INFO, "app_message_open() called with app_message_inbox_size_maximum().");
    APP_LOG(LOG_LEVEL_INFO,
            "This consumes %"PRIu32" bytes of heap memory, potentially more in the future!",
            (uint32_t)size_maximum);

  }
  if (size_inbound == 0) {
    return APP_MSG_OK;
  }
  // Add extra space needed for protocol overhead:
  if (!app_message_receiver_open(size_inbound + APP_MSG_HDR_OVRHD_SIZE)) {
    return APP_MSG_OUT_OF_MEMORY;
  }
  inbox->is_open = true;

  return APP_MSG_OK;
}

void app_message_inbox_close(AppMessageCtxInbox *inbox) {
  app_message_receiver_close();
  inbox->is_open = false;
}

void app_message_inbox_send_ack_nack_reply(CommSession *session, const uint8_t transaction_id,
                                           AppMessageCmd cmd) {
  const AppMessageAck nack_message = (const AppMessageAck) {
    .header = {
      .command = cmd,
      .transaction_id = transaction_id,
    },
  };
  // Just use a syscall to enqueue the message using kernel heap.
  // We could use app_outbox, but then we'd need to allocate the message on the app heap and I'm
  // afraid this might break apps, especially if the mobile app is misbehaving and avalanching the
  // app with messages that need to be (n)ack'd.
  sys_app_pp_send_data(session, APP_MESSAGE_ENDPOINT_ID,
                       (const uint8_t *) &nack_message, sizeof(nack_message));
}

void app_message_inbox_handle_dropped_messages(uint32_t num_drops) {
  // Taking a shortcut here. We used to report either APP_MSG_BUFFER_OVERFLOW or APP_MSG_BUSY back
  // to the app. With the new the Receiver / AppInbox system, there are different reasons why
  // messages get dropped. Just map everything to "APP_MSG_BUSY":
  AppMessageCtx *app_message_ctx = app_state_get_app_message_ctx();
  AppMessageCtxInbox *inbox = &app_message_ctx->inbox;
  const bool is_open_and_has_handler = (inbox->is_open && inbox->dropped_callback);
  for (uint32_t i = 0; i < num_drops; ++i) {
    if (is_open_and_has_handler) {
      inbox->dropped_callback(APP_MSG_BUSY, inbox->user_context);
    }
  }
}

static bool prv_is_app_with_uuid_running(const Uuid *uuid) {
  Uuid app_uuid = {};
  sys_get_app_uuid(&app_uuid);
  return uuid_equal(&app_uuid, uuid);
}

void app_message_inbox_receive(CommSession *session, AppMessagePush *push_message, size_t length,
                               AppInboxConsumerInfo *consumer_info) {
  // Test if the data is long enough to contain a push message:
  if (length < sizeof(AppMessagePush)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Too short");
    return;
  }

  AppMessageCtxInbox *inbox = &app_state_get_app_message_ctx()->inbox;
  const uint8_t transaction_id = push_message->header.transaction_id;

  // Verify UUID for app-bound messages:
  if (!prv_is_app_with_uuid_running(&push_message->uuid)) {
    app_message_inbox_send_ack_nack_reply(session, transaction_id, CMD_NACK);
    sys_app_pp_app_message_analytics_count_drop();
    return;
  }

  DictionaryIterator iterator;
  const uint16_t dict_size = (length - APP_MSG_HDR_OVRHD_SIZE);
  // TODO PBL-1639: Maybe do some sanity checking on the dict structure?
  dict_read_begin_from_buffer(&iterator, (const uint8_t *) &push_message->dictionary, dict_size);

  if (inbox->received_callback) {
    inbox->received_callback(&iterator, inbox->user_context);
  }

  // Mark data as consumed...
  app_inbox_consume(consumer_info);

  // ... only then send the ACK:
  app_message_inbox_send_ack_nack_reply(session, transaction_id, CMD_ACK);

  s_app_message_received_count++;
}

uint32_t app_message_inbox_get_received_count(void) {
  return s_app_message_received_count;
}
