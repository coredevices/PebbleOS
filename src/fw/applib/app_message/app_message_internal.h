/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_message/app_message.h"
#include "applib/app_timer.h"
#include "services/normal/app_message/app_message_sender.h"
#include "util/attributes.h"
#include "util/uuid.h"

typedef struct CommSession CommSession;

#define ACK_NACK_TIME_OUT_MS          (10000)
#define APP_MESSAGE_ENDPOINT_ID       (0x30)

typedef enum {
  CMD_PUSH = 0x01,
  CMD_REQUEST = 0x02,
  CMD_ACK = 0xff,
  CMD_NACK = 0x7f,
} AppMessageCmd;

typedef struct PACKED {
  AppMessageCmd command:8;
  uint8_t transaction_id;
} AppMessageHeader;

//! The actual wire format of an app message message
typedef struct PACKED {
  AppMessageHeader header;
  Uuid uuid;
  Dictionary dictionary; //!< Variable length!
} AppMessagePush;
// AppMessageHeader and Uuid size should be opaque to user of API
#define APP_MSG_HDR_OVRHD_SIZE (offsetof(AppMessagePush, dictionary))

#define APP_MSG_8K_DICT_SIZE (sizeof(Dictionary) + sizeof(Tuple) + (8 * 1024))

typedef struct PACKED {
  AppMessageHeader header;
} AppMessageAck;

// For a diagram of the state machine:
// https://pebbletechnology.atlassian.net/wiki/pages/editpage.action?pageId=91914242

typedef enum AppMessagePhaseOut {
  //! The App Message Outbox is not enabled.  To enable it, call app_message_open().
  OUT_CLOSED = 0,
  //! The dictionary writing can be "started" by calling app_message_outbox_begin()
  OUT_ACCEPTING,
  //! app_message_outbox_begin() has been called and the dictionary can be written and then sent.
  OUT_WRITING,
  //! app_message_outbox_send() has been called. The ack/nack timeout timer has been set and
  //! we're awaiting an ack/nack on the sent message AND the callback from the AppOutbox subsystem
  //! that the data has been consumed. These 2 things happen in parallel, the order in which they
  //! happen is undefined.
  OUT_AWAITING_REPLY_AND_OUTBOX_CALLBACK,
  //! We're still awaiting the AppMessage ack/nack, but the AppOutbox subsystem has indicated that
  //! the data has been consumed.
  OUT_AWAITING_REPLY,
  //! We're still awaiting the callback from the AppOutbox subsystem to indicate the data has been
  //! consumed, but we have already received the AppMessage ack/nack. This state is possible because
  //! acking at a lower layer (i.e. PPoGATT) can happen with a slight delay.
  OUT_AWAITING_OUTBOX_CALLBACK,
} AppMessagePhaseOut;

typedef struct AppMessageCtxInbox {
  bool is_open;
  void *user_context;
  AppMessageInboxReceived received_callback;
  AppMessageInboxDropped dropped_callback;
} AppMessageCtxInbox;

typedef struct AppMessageCtxOutbox {
  DictionaryIterator iterator;
  size_t transmission_size_limit;

  AppMessageAppOutboxData *app_outbox_message;

  AppMessageOutboxSent sent_callback;
  AppMessageOutboxFailed failed_callback;
  void *user_context;

  AppTimer *ack_nack_timer;

  struct PACKED {
    AppMessagePhaseOut phase:8;
    uint8_t transaction_id;
    uint16_t not_ready_throttle_ms;       // used for throttling app task when outbox is not ready
    AppMessageResult result:16;
  };
} AppMessageCtxOutbox;

typedef struct AppMessageCtx {
  AppMessageCtxInbox inbox;
  AppMessageCtxOutbox outbox;
} AppMessageCtx;

_Static_assert(sizeof(AppMessageCtx) <= 112,
               "AppMessageCtx must not exceed 112 bytes!");

typedef struct {
  CommSession *session;
  //! To give us some room for future changes. This structure ends up in a buffer that is sized by
  //! the app, so we can't easily increase the size of this once shipped.
  uint8_t padding[8];
  uint8_t data[];
} AppMessageReceiverHeader;

#ifndef UNITTEST
_Static_assert(sizeof(AppMessageReceiverHeader) == 12,
               "The size of AppMessageReceiverHeader cannot grow beyond 12 bytes!");
#endif

void app_message_init(void);

AppMessageResult app_message_inbox_open(AppMessageCtxInbox *inbox, size_t size_inbound);

void app_message_inbox_close(AppMessageCtxInbox *inbox);

typedef struct AppInboxConsumerInfo AppInboxConsumerInfo;

void app_message_inbox_receive(CommSession *session, AppMessagePush *push_message, size_t length,
                               AppInboxConsumerInfo *consumer_info);

AppMessageResult app_message_outbox_open(AppMessageCtxOutbox *outbox, size_t size_outbound);

void app_message_outbox_close(AppMessageCtxOutbox *outbox);

void app_message_out_handle_ack_nack_received(const AppMessageHeader *header);

void app_message_inbox_send_ack_nack_reply(CommSession *session, const uint8_t transaction_id,
                                           AppMessageCmd cmd);

void app_message_inbox_handle_dropped_messages(uint32_t num_drops);

void app_message_app_protocol_msg_callback(CommSession *session,
                                           const uint8_t* data, size_t length,
                                           AppInboxConsumerInfo *consumer_info);

void app_message_app_protocol_system_nack_callback(CommSession *session,
                                                   const uint8_t* data, size_t length);

//! Get the count of AppMessages sent (for Memfault metrics)
uint32_t app_message_outbox_get_sent_count(void);

//! Get the count of AppMessages received (for Memfault metrics)
uint32_t app_message_inbox_get_received_count(void);
