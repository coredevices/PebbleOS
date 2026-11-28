/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/endpoint.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/pp/session.h>
#include <pbl/services/blob_db/chat_db.h>
#include <pbl/services/chat/chat_service.h>

#include <applib/dict.h>
#include <apps/system/chat/chat_keys.h>
#include <kernel/pebble_tasks.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>
#include <process_management/process_manager.h>
#include <shell/system_app_ids.auto.h>

static ChatServiceHandler s_handler;
// Set on KernelMain (a notification's action), taken on the app task: copied whole under the lock
static PBL_MUTEX_DEFINE(s_hint_lock);
static ChatLaunchHint s_launch_hint;
static bool s_has_launch_hint;
static ChatServiceHintHandler s_hint_handler;

typedef struct {
  size_t len;
  uint8_t dict[];
} Packet;

static void prv_deliver(void *data) {
  Packet *packet = data;
  // Read again on the app task: the app may have exited since the packet was queued
  if (s_handler) {
    s_handler(packet->dict, packet->len);
  }
  pbl_free(packet);
}

// A string tuple of the phone's dictionary, or nullptr: checked against its length, not trusted
// (Pebble dictionary: count u8, then key u32, type u8, length u16, value per tuple)
static const char *prv_dict_cstring(const uint8_t *dict, size_t len, uint32_t key) {
  if (len < 1) {
    return nullptr;
  }
  const uint8_t *p = dict + 1, *end = dict + len;
  for (int i = 0; i < dict[0]; i++) {
    if (end - p < 7) {
      return nullptr;
    }
    const uint32_t k = p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
    const uint8_t type = p[4];
    const uint16_t n = p[5] | (p[6] << 8);
    p += 7;
    if (end - p < n) {
      return nullptr;
    }
    if (k == key) {
      return (type == TUPLE_CSTRING && n > 0 && p[n - 1] == '\0') ? (const char *)p : nullptr;
    }
    p += n;
  }
  return nullptr;
}

// The phone's "open_chat" (Index: a message to someone not in the favourites, for the user to
// send): the Chat app opens on that chat with the message in its bar, launched if it isn't running
static bool prv_open_chat_from_phone(const uint8_t *dict, size_t len) {
  const char *type = prv_dict_cstring(dict, len, MESSAGE_KEY_Type);
  const char *chat_id = prv_dict_cstring(dict, len, MESSAGE_KEY_ChatId);
  const char *text = prv_dict_cstring(dict, len, MESSAGE_KEY_Text);
  if (!type || strcmp(type, "open_chat") != 0 || !chat_id) {
    return false;
  }
  ChatLaunchHint *hint = pbl_zalloc(sizeof(ChatLaunchHint));
  if (!hint) {
    return true;
  }
  strncpy(hint->chat_id, chat_id, sizeof(hint->chat_id) - 1);
  strncpy(hint->draft, text ? text : "", sizeof(hint->draft) - 1);
  const bool running = s_hint_handler != nullptr;
  chat_service_set_launch_hint(hint);
  pbl_free(hint);
#if defined(APP_ID_CHAT)
  if (!running && APP_ID_CHAT != INSTALL_ID_INVALID) {
    app_manager_put_launch_app_event(&(AppLaunchEventConfig){.id = APP_ID_CHAT});
  }
#endif
  return true;
}

static void prv_handle_packet(struct pbl_pp_session *session, const uint8_t *data, size_t length) {
  if (length < 1 || data[0] != CHAT_SERVICE_VERSION) {
    PBL_LOG_WRN("Chat: dropping version %u", length ? data[0] : 0);
    return;
  }
  if (prv_open_chat_from_phone(data + 1, length - 1)) {
    return;
  }
  if (!s_handler) {
    return; // Chat app not running; it reads what it needs from the chat db when it starts
  }
  Packet *packet = pbl_malloc(sizeof(Packet) + length - 1);
  if (!packet) {
    return;
  }
  packet->len = length - 1;
  memcpy(packet->dict, data + 1, length - 1);
  // ponytail: a packet queued just as the app exits is never freed; a per-app queue drained on
  // exit if it shows up in heap stats.
  process_manager_send_callback_event_to_process(PebbleTask_App, prv_deliver, packet);
}

#ifndef CONFIG_RECOVERY_FW
PBL_PP_ENDPOINT_DEFINE(PBL_PP_ENDPOINT_CHAT, PBL_PP_ACCESS_PRIVATE, prv_handle_packet);
#endif

void chat_service_set_handler(ChatServiceHandler handler) {
  s_handler = handler;
}

bool chat_service_send(const uint8_t *dict, size_t len) {
  struct pbl_pp_session *session = pbl_pp_session_get_system();
  if (!session) {
    return false;
  }
  struct pbl_pp_send_buffer *sb = pbl_pp_send_buffer_begin(session, PBL_PP_ENDPOINT_CHAT, len + 1,
                                                           PBL_PP_DEFAULT_TIMEOUT_MS);
  if (!sb) {
    return false;
  }
  const uint8_t version = CHAT_SERVICE_VERSION;
  pbl_pp_send_buffer_write(sb, &version, 1);
  pbl_pp_send_buffer_write(sb, dict, len);
  pbl_pp_send_buffer_end(sb);
  return true;
}

static void prv_deliver_hint(void *data) {
  if (s_hint_handler) {
    s_hint_handler();
  }
}

void chat_service_set_launch_hint(const ChatLaunchHint *hint) {
  pbl_mutex_lock(&s_hint_lock, PBL_FOREVER);
  s_has_launch_hint = hint != nullptr;
  if (hint) {
    s_launch_hint = *hint;
  }
  pbl_mutex_unlock(&s_hint_lock);
  if (s_hint_handler) {  // the app is running: it reads the hint now, not at its next launch
    process_manager_send_callback_event_to_process(PebbleTask_App, prv_deliver_hint, nullptr);
  }
}

void chat_service_set_hint_handler(ChatServiceHintHandler handler) {
  s_hint_handler = handler;
}

bool chat_service_take_launch_hint(ChatLaunchHint *out) {
  pbl_mutex_lock(&s_hint_lock, PBL_FOREVER);
  const bool has = s_has_launch_hint;
  if (has) {
    *out = s_launch_hint;
  }
  s_has_launch_hint = false;
  pbl_mutex_unlock(&s_hint_lock);
  return has;
}

typedef struct {
  const char *title;
  uint32_t newest;
  uint8_t key[CHAT_DB_CHAT_KEY_LEN];
  bool found;
} TitleMatch;

static bool prv_match_title(const uint8_t *key, int key_len, const uint8_t *val, int val_len,
                            void *context) {
  TitleMatch *match = context;
  ChatDbChat chat;
  if (key_len != 1 + CHAT_DB_CHAT_KEY_LEN || key[0] != CHAT_DB_KIND_CHAT ||
      !chat_db_parse_chat(val, val_len, &chat)) {
    return true;
  }
  // Two chats can share a name (one person on two networks): the newest is the one notifying
  if (chat.title.len == strlen(match->title) &&
      memcmp(chat.title.str, match->title, chat.title.len) == 0 &&
      (!match->found || chat.last_activity >= match->newest)) {
    match->found = true;
    match->newest = chat.last_activity;
    memcpy(match->key, key + 1, CHAT_DB_CHAT_KEY_LEN);
  }
  return true;
}

bool chat_service_archive_chat_titled(const char *title) {
  if (!title || !title[0]) {
    return false;
  }
  TitleMatch match = {.title = title};
  chat_db_each(prv_match_title, &match);
  if (!match.found) {
    return false;
  }
  // The Chat app's chat id: the chat key in lowercase hex
  static const char digits[] = "0123456789abcdef";
  char id[CHAT_DB_CHAT_KEY_LEN * 2 + 1];
  for (int i = 0; i < CHAT_DB_CHAT_KEY_LEN; i++) {
    id[i * 2] = digits[match.key[i] >> 4];
    id[i * 2 + 1] = digits[match.key[i] & 0xF];
  }
  id[CHAT_DB_CHAT_KEY_LEN * 2] = '\0';

  uint8_t buf[64];
  DictionaryIterator iter;
  dict_write_begin(&iter, buf, sizeof(buf));
  dict_write_cstring(&iter, MESSAGE_KEY_Command, "archive_chat");
  dict_write_cstring(&iter, MESSAGE_KEY_ChatId, id);
  return chat_service_send(buf, dict_write_end(&iter));
}
