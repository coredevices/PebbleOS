/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "launcher_app_message.h"

#include "app_run_state.h"
#include "applib/app_message/app_message_internal.h"
#include "process_management/app_install_manager.h"
#include "system/logging.h"
#include "system/passert.h"

#include "util/dict.h"

#define LAUNCHER_MESSAGE_ENDPOINT_ID  (0x31)

typedef enum {
  //! Used as reply from the watch to the phone, to indicate the app is not running.
  //! Or, when pushed from phone to watch, this value will have the effect of killing the app.
  NOT_RUNNING_DEPRECATED = 0x00,
  //! Used as reply from the watch to the phone, to indicate the app is running.
  //! Or, when pushed from phone to watch, this value will have the effect of launching the app.
  RUNNING_DEPRECATED = 0x01,
} AppStateDeprecated;

enum {
  //! This key/value can be pushed from the phone to the watch to launch
  //! or kill an app on the watch.
  RUN_STATE_KEY = 0x01, // TUPLE_UINT8
  STATE_FETCH_KEY = 0x02, // TUPLE_UINT8
};

static uint8_t s_transaction_id;

// For unit testing
void launcher_app_message_reset(void) {
  s_transaction_id = 0;
}

void launcher_app_message_send_app_state_deprecated(const Uuid *uuid, bool running) {
  // Deprecated: 0x31 endpoint, only used by Android versions < 2.2 and iOS
  AppStateDeprecated app_state = running ? RUNNING_DEPRECATED : NOT_RUNNING_DEPRECATED;

  uint8_t buffer[sizeof(AppMessagePush) + sizeof(Tuple) + sizeof(uint32_t)];
  AppMessagePush *push_message = (AppMessagePush *)buffer;

  *push_message = (const AppMessagePush) {
    .header = {
      .command = CMD_PUSH,
      .transaction_id = s_transaction_id++,
    },
    .uuid = *uuid,
  };

  uint32_t size = sizeof(Dictionary) + sizeof(Tuple) + sizeof(uint32_t);
  const Tuplet tuplet = TupletInteger(RUN_STATE_KEY, (uint32_t) app_state);
  PBL_ASSERTN(DICT_OK == dict_serialize_tuplets_to_buffer(
                                &tuplet, 1, (uint8_t *)&push_message->dictionary, &size));

  comm_session_send_data(comm_session_get_system_session(), LAUNCHER_MESSAGE_ENDPOINT_ID,
                         (const uint8_t *)buffer, sizeof(buffer), COMM_SESSION_DEFAULT_TIMEOUT);
}

static bool prv_has_invalid_length(size_t expected, size_t actual) {
  if (actual < expected) {
    PBL_LOG(LOG_LEVEL_ERROR, "Too short");
    return true;
  }
  return false;
}

static bool prv_receive_push_cmd(CommSession *session,
                                 AppMessagePush *push_message, size_t length) {
  if (prv_has_invalid_length(sizeof(AppMessagePush), length)) {
    return false;
  }

  bool success = false;

  // Scan the dictionary:
  const size_t dict_size = length - sizeof(AppMessagePush) + sizeof(Dictionary);
  DictionaryIterator iter;
  const Tuple *tuple = dict_read_begin_from_buffer(&iter,
                                                   (const uint8_t *) &push_message->dictionary,
                                                   dict_size);
  while (tuple) {
    uint8_t cmd = tuple->key;
    switch (cmd) {
      case RUN_STATE_KEY:
        if (tuple->value->uint8 != RUNNING_DEPRECATED) {
          cmd = APP_RUN_STATE_STOP_COMMAND;
        } else {
          cmd = APP_RUN_STATE_RUN_COMMAND;
        }
        success = true;
        break;
      case STATE_FETCH_KEY:
        cmd = APP_RUN_STATE_STATUS_COMMAND;
        success = true;
        break;
      default:
        cmd = APP_RUN_STATE_INVALID_COMMAND;
    }

    // Call into app_run_state to take the action (to avoid duping code):
    const Uuid *app_uuid = &push_message->uuid;
    app_run_state_command(NULL, (AppRunStateCommand)cmd, app_uuid);
    tuple = dict_read_next(&iter);
  }

  return success;
}

static void prv_send_ack_nack_reply(CommSession *session, const uint8_t transaction_id, bool ack) {
  const AppMessageAck nack_message = {
    .header = {
      .command = ack ? CMD_ACK : CMD_NACK,
      .transaction_id = transaction_id,
    },
  };
  comm_session_send_data(session, LAUNCHER_MESSAGE_ENDPOINT_ID,
                         (const uint8_t *) &nack_message, sizeof(nack_message),
                         COMM_SESSION_DEFAULT_TIMEOUT);
}

void launcher_app_message_protocol_msg_callback_deprecated(CommSession *session,
                                                           const uint8_t* data, size_t length) {
  if (prv_has_invalid_length(sizeof(AppMessageHeader), length)) {
    return;
  }

  AppMessageHeader *message = (AppMessageHeader *) data;
  bool ack = false;
  switch (message->command) {
    case CMD_PUSH: {
      // Incoming message:
      ack = prv_receive_push_cmd(session, (AppMessagePush *) message, length);
      break;
    }

    case CMD_ACK:
    case CMD_NACK:
      // Ignore ACK / NACKs
      return;

    default:
      // Ignore everything else
      break;
  }

  prv_send_ack_nack_reply(session, message->transaction_id, ack);
}
