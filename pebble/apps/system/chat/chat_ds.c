/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "chat_ds.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <pbl/kernel/time.h>
#include <pbl/pp/session.h>
#include <pbl/services/blob_db/api.h>
#include <pbl/services/blob_db/chat_db.h>
#include <pbl/services/blob_db/ios_notif_pref_db.h>
#include <pbl/services/chat/chat_service.h>
#include <pbl/services/clock.h>
#include <pbl/services/notifications/notification_constants.h>
#include <pbl/services/timeline/attribute.h>
#include <pbl/util/string_list.h>

#include <applib/app_malloc.h>
#include <applib/app_timer.h>
#include <applib/event_service_client.h>
#include <applib/pbl_std/pbl_std.h>
#include <kernel/events.h>

#include "chat_keys.h"

#define MAX_CHATS 20
#define AVATAR_REQUESTS MAX_CHATS  // every listed chat, as the watchapp got
#define AVATAR_FIRST_DELAY_MS 500  // after the chat a notification opened has been asked for
#define AVATAR_GAP_MS 250
#define MAX_ROWS 32
#define WINDOW_ROWS 9
#define OUT_SIZE 700
#define IN_SIZE 1400
#define CHANGE_DELAY_MS 300
#define STALE_SECS (5 * 60)

// Field limits of the watchapp's phone side (chat/pbw/src/pkjs/index.js), which the UI expects.
#define MAX_ID_BYTES 23
#define MAX_TITLE_BYTES 47
#define MAX_PREVIEW_BYTES 71
#define MAX_SENDER_BYTES 35
#define MAX_TEXT_BYTES 460
#define MAX_REACTIONS_BYTES 63
#define MAX_REPLY_BYTES 95
#define MAX_META_BYTES 15

#define CHAT_KEY_HEX_LEN (CHAT_DB_CHAT_KEY_LEN * 2)

typedef enum {
  ModeInitial = 0,
  ModeOlder = 1,
  ModeNewer = 2,
} StreamMode;

typedef struct {
  uint8_t key[1 + CHAT_DB_CHAT_KEY_LEN + CHAT_DB_MESSAGE_KEY_LEN];
  uint8_t key_len;
  uint32_t time;
} RowRef;

static struct {
  AppMessageInboxReceived inbox_received;
  AppMessageOutboxSent outbox_sent;
  AppMessageOutboxFailed outbox_failed;

  DictionaryIterator out_iter;
  uint8_t out[OUT_SIZE];
  bool out_busy;

  uint8_t in[IN_SIZE];

  EventServiceInfo blob_db_event;
  AppTimer *change_timer;

  bool chat_open;
  uint8_t open_chat[CHAT_DB_CHAT_KEY_LEN];
  uint32_t newest_shown;
  int transfer_seq;

  char launch_hint[64];

  // Chat pictures: asked for one at a time, each chat once per launch
  AppTimer *avatar_timer;
  uint8_t avatar_queue[AVATAR_REQUESTS][CHAT_DB_CHAT_KEY_LEN];
  int avatar_queue_len;
  int avatar_queue_next;
  uint8_t avatar_asked[AVATAR_REQUESTS][CHAT_DB_CHAT_KEY_LEN];
  int avatar_asked_count;
} s_ds;

////////////////////////////
// Strings
////////////////////////////

// Copy at most max_bytes of s into out (NUL terminated) without splitting a UTF-8 sequence.
static void prv_copy_str(char *out, const char *s, size_t len, size_t max_bytes) {
  if (len > max_bytes) {
    len = max_bytes;
    while (len > 0 && ((uint8_t)s[len] & 0xC0) == 0x80) {
      len--;
    }
  }
  memcpy(out, s, len);
  out[len] = '\0';
}

static void prv_write_str(DictionaryIterator *iter, uint32_t key, ChatDbStr str, size_t max_bytes) {
  char buf[MAX_TEXT_BYTES + 1];
  prv_copy_str(buf, str.str ? str.str : "", str.len, max_bytes);
  dict_write_cstring(iter, key, buf);
}

static void prv_hex(char *out, const uint8_t *bytes, size_t len) {
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = digits[bytes[i] >> 4];
    out[i * 2 + 1] = digits[bytes[i] & 0xF];
  }
  out[len * 2] = '\0';
}

static bool prv_unhex(uint8_t *out, const char *hex) {
  if (!hex || strlen(hex) != CHAT_KEY_HEX_LEN) {
    return false;
  }
  for (int i = 0; i < CHAT_KEY_HEX_LEN; i++) {
    const char c = hex[i];
    const int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
    if (v < 0) {
      return false;
    }
    out[i / 2] = (i % 2) ? (out[i / 2] | v) : (v << 4);
  }
  return true;
}

static bool prv_str_eq(ChatDbStr str, const char *cstr) {
  return cstr && str.len == strlen(cstr) && memcmp(str.str, cstr, str.len) == 0;
}

// Pebblegram's chat list time: time today, weekday this week, else month/day.
static void prv_chat_time(char *out, size_t size, uint32_t utc) {
  if (!utc) {
    out[0] = '\0';
    return;
  }
  const time_t now = pbl_realtime();
  struct pbl_tm today = *pbl_override_localtime(&now);
  const time_t t = utc;
  struct pbl_tm when = *pbl_override_localtime(&t);
  const int days_ago = (today.tm_yday - when.tm_yday + 366) % 366;
  static const char *const weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  if (when.tm_year == today.tm_year && days_ago == 0) {
    snprintf(out, size, "%u:%02u", (uint8_t)when.tm_hour, (uint8_t)when.tm_min);
  } else if (days_ago < 7 && now - t < 7 * 86400) {
    snprintf(out, size, "%s", weekdays[when.tm_wday]);
  } else {
    snprintf(out, size, "%u/%u", (uint8_t)(when.tm_mon + 1), (uint8_t)when.tm_mday);
  }
}

static void prv_message_time(char *out, size_t size, uint32_t utc) {
  const time_t t = utc;
  struct pbl_tm when = *pbl_override_localtime(&t);
  snprintf(out, size, "%u:%02u", (uint8_t)when.tm_hour, (uint8_t)when.tm_min);
}

////////////////////////////
// Reading the chat db
////////////////////////////

typedef struct {
  char kind;
  const uint8_t *chat_key; // messages of this chat only
  RowRef *rows;
  int count;
  int cap;
} Collect;

static uint32_t prv_row_time(const uint8_t *key, const uint8_t *val, int val_len) {
  if (key[0] == CHAT_DB_KIND_CHAT) {
    ChatDbChat chat;
    return chat_db_parse_chat(val, val_len, &chat) ? chat.last_activity : 0;
  }
  ChatDbMessage message;
  return chat_db_parse_message(val, val_len, &message) ? message.time : 0;
}

// Keeps the cap newest rows of a kind.
static bool prv_collect(const uint8_t *key, int key_len, const uint8_t *val, int val_len,
                        void *context) {
  Collect *c = context;
  if (key[0] != c->kind || key_len > (int)sizeof(c->rows[0].key)) {
    return true;
  }
  if (c->chat_key && memcmp(key + 1, c->chat_key, CHAT_DB_CHAT_KEY_LEN) != 0) {
    return true;
  }
  RowRef row = {.key_len = key_len, .time = prv_row_time(key, val, val_len)};
  memcpy(row.key, key, key_len);
  if (c->count < c->cap) {
    c->rows[c->count++] = row;
    return true;
  }
  int oldest = 0;
  for (int i = 1; i < c->count; i++) {
    if (c->rows[i].time < c->rows[oldest].time) {
      oldest = i;
    }
  }
  if (row.time > c->rows[oldest].time) {
    c->rows[oldest] = row;
  }
  return true;
}

static int prv_compare_time(const void *a, const void *b) {
  const uint32_t ta = ((const RowRef *)a)->time;
  const uint32_t tb = ((const RowRef *)b)->time;
  return (ta > tb) - (ta < tb);
}

// Rows of a kind, oldest first.
static int prv_rows(char kind, const uint8_t *chat_key, RowRef *rows, int cap) {
  Collect c = {.kind = kind, .chat_key = chat_key, .rows = rows, .cap = cap};
  chat_db_each(prv_collect, &c);
  qsort(rows, c.count, sizeof(RowRef), prv_compare_time);
  return c.count;
}

// The record's value, app heap allocated; NULL if gone.
static uint8_t *prv_read(const RowRef *row, int *len_out) {
  const int len = chat_db_get_len(row->key, row->key_len);
  if (len <= 0) {
    return nullptr;
  }
  uint8_t *val = app_malloc(len);
  if (val && chat_db_read(row->key, row->key_len, val, len) != 0) {
    app_free(val);
    val = nullptr;
  }
  *len_out = len;
  return val;
}

////////////////////////////
// Delivering to the UI
////////////////////////////

static DictionaryIterator *prv_begin(const char *type) {
  static DictionaryIterator iter;
  dict_write_begin(&iter, s_ds.in, sizeof(s_ds.in));
  dict_write_cstring(&iter, MESSAGE_KEY_Type, type);
  return &iter;
}

static void prv_deliver(DictionaryIterator *iter) {
  const uint32_t len = dict_write_end(iter);
  if (!s_ds.inbox_received || len == 0) {
    return;
  }
  DictionaryIterator read;
  dict_read_begin_from_buffer(&read, s_ds.in, len);
  s_ds.inbox_received(&read, nullptr);
}

static void prv_deliver_simple(const char *type, uint32_t key, const char *value) {
  DictionaryIterator *iter = prv_begin(type);
  if (value) {
    dict_write_cstring(iter, key, value);
  }
  prv_deliver(iter);
}

static void prv_deliver_done(const char *type, int count, int transfer_id) {
  DictionaryIterator *iter = prv_begin(type);
  dict_write_int32(iter, MESSAGE_KEY_Count, count);
  if (transfer_id) {
    dict_write_int32(iter, MESSAGE_KEY_ImageTransferId, transfer_id);
  }
  prv_deliver(iter);
}

static void prv_deliver_message(const char *type, int index, int count, int transfer_id,
                                const ChatDbMessage *m) {
  char meta[MAX_META_BYTES + 1];
  prv_message_time(meta, sizeof(meta), m->time);
  DictionaryIterator *iter = prv_begin(type);
  dict_write_int32(iter, MESSAGE_KEY_Index, index);
  dict_write_int32(iter, MESSAGE_KEY_Count, count);
  if (transfer_id) {
    dict_write_int32(iter, MESSAGE_KEY_ImageTransferId, transfer_id);
  }
  prv_write_str(iter, MESSAGE_KEY_MessageId, m->id, MAX_ID_BYTES);
  prv_write_str(iter, MESSAGE_KEY_Sender, m->sender, MAX_SENDER_BYTES);
  prv_write_str(iter, MESSAGE_KEY_Text, m->text, MAX_TEXT_BYTES);
  if (m->reactions.len) {
    prv_write_str(iter, MESSAGE_KEY_Reactions, m->reactions, MAX_REACTIONS_BYTES);
  }
  dict_write_cstring(iter, MESSAGE_KEY_MessageMeta, meta);
  if (m->reply_text.len || m->reply_sender.len) {
    prv_write_str(iter, MESSAGE_KEY_ReplySender, m->reply_sender, MAX_SENDER_BYTES);
    prv_write_str(iter, MESSAGE_KEY_ReplyText, m->reply_text, MAX_REPLY_BYTES);
  }
  dict_write_int32(iter, MESSAGE_KEY_IsOutgoing, (m->flags & ChatDbMessageFlagOutgoing) ? 1 : 0);
  prv_deliver(iter);
}

////////////////////////////
// Chat list
////////////////////////////

static void prv_deliver_settings(void) {
  char features[128] = "";
  const uint8_t key = CHAT_DB_KIND_FEATURES;
  uint8_t val[sizeof(features)];
  const int len = chat_db_get_len(&key, 1);
  if (len > 1 && len <= (int)sizeof(val) && chat_db_read(&key, 1, val, len) == 0) {
    prv_copy_str(features, (const char *)val + 1, val[0], sizeof(features) - 1);
  }

  // The canned replies the phone app set for notification replies
  char canned[200] = "";
  iOSNotifPrefs *prefs = ios_notif_pref_db_get_prefs((const uint8_t *)SEND_TEXT_NOTIF_PREF_KEY,
                                                     strlen(SEND_TEXT_NOTIF_PREF_KEY));
  for (int i = 0; prefs && i < prefs->action_group.num_actions; i++) {
    struct pbl_string_list *list = attribute_get_string_list(
        &prefs->action_group.actions[i].attr_list, AttributeIdCannedResponses);
    const size_t n = list ? pbl_string_list_count(list) : 0;
    for (size_t j = 0; j < n; j++) {
      const size_t used = strlen(canned);
      snprintf(canned + used, sizeof(canned) - used, "%s%s", used ? "|" : "",
               pbl_string_list_get_at(list, j));
    }
    if (n) {
      break;
    }
  }
  if (prefs) {
    ios_notif_pref_db_free_prefs(prefs);
  }

  DictionaryIterator *iter = prv_begin("settings");
  if (canned[0]) {
    dict_write_cstring(iter, MESSAGE_KEY_CannedReplies, canned);
  }
  dict_write_cstring(iter, MESSAGE_KEY_Text, features);
  prv_deliver(iter);
}

// Answers get_chats. Returns the id of the newest chat whose title is the launch hint, if any
// (two chats can share a name, e.g. the same person on two networks).
static void prv_deliver_chats(char *hint_chat_id) {
  RowRef rows[MAX_CHATS];
  const int count = prv_rows(CHAT_DB_KIND_CHAT, nullptr, rows, MAX_CHATS);
  uint32_t oldest_sync = UINT32_MAX;
  int delivered = 0;
  for (int i = count - 1; i >= 0; i--) { // newest first
    int len;
    uint8_t *val = prv_read(&rows[i], &len);
    ChatDbChat chat;
    if (!val || !chat_db_parse_chat(val, len, &chat)) {
      app_free(val);
      continue;
    }
    char id[CHAT_KEY_HEX_LEN + 1];
    prv_hex(id, rows[i].key + 1, CHAT_DB_CHAT_KEY_LEN);
    char meta[MAX_META_BYTES + 1];
    char when[12];
    prv_chat_time(when, sizeof(when), chat.last_activity);
    snprintf(meta, sizeof(meta), "%s|%s%s", when, (chat.flags & ChatDbChatFlagGroup) ? "g" : "",
             (chat.flags & ChatDbChatFlagMuted) ? "m" : "");
    if (hint_chat_id && !hint_chat_id[0] && s_ds.launch_hint[0] && prv_str_eq(chat.title, s_ds.launch_hint)) {
      strcpy(hint_chat_id, id);
    }
    if (chat.synced < oldest_sync) {
      oldest_sync = chat.synced;
    }

    DictionaryIterator *iter = prv_begin("chat");
    dict_write_int32(iter, MESSAGE_KEY_Index, delivered);
    dict_write_int32(iter, MESSAGE_KEY_Count, count);
    dict_write_cstring(iter, MESSAGE_KEY_ChatId, id);
    prv_write_str(iter, MESSAGE_KEY_Sender, chat.title, MAX_TITLE_BYTES);
    prv_write_str(iter, MESSAGE_KEY_Text, chat.preview, MAX_PREVIEW_BYTES);
    dict_write_int32(iter, MESSAGE_KEY_IsUnread, chat.unread_count > 0);
    dict_write_int32(iter, MESSAGE_KEY_UnreadCount, chat.unread_count);
    dict_write_cstring(iter, MESSAGE_KEY_MessageMeta, meta);
    app_free(val);
    prv_deliver(iter);
    delivered++;
  }
  prv_deliver_done("chats_done", delivered, 0);

  // Say how old the list is when the phone hasn't refreshed it lately
  const uint32_t now = pbl_realtime();
  const bool connected = pbl_pp_session_get_system() != nullptr;
  if (delivered && (!connected || now - oldest_sync > STALE_SECS)) {
    char status[32];
    const uint32_t mins = (now - oldest_sync) / 60;
    if (mins < 60) {
      snprintf(status, sizeof(status), "Updated %lum ago", (unsigned long)mins);
    } else if (mins < 48 * 60) {
      snprintf(status, sizeof(status), "Updated %luh ago", (unsigned long)(mins / 60));
    } else {
      snprintf(status, sizeof(status), "Updated %lud ago", (unsigned long)(mins / 1440));
    }
    prv_deliver_simple("status", MESSAGE_KEY_Status, status);
  }
}

////////////////////////////
// Messages
////////////////////////////

static void prv_note_shown(const ChatDbMessage *m) {
  if (m->time > s_ds.newest_shown) {
    s_ds.newest_shown = m->time;
  }
}

static int prv_start_stream(StreamMode mode, bool silent, int count) {
  const int transfer_id = ++s_ds.transfer_seq;
  DictionaryIterator *iter = prv_begin("messages_start");
  dict_write_int32(iter, MESSAGE_KEY_Count, count);
  dict_write_int32(iter, MESSAGE_KEY_Index, mode);
  if (silent) {
    dict_write_cstring(iter, MESSAGE_KEY_Text, "silent");
  }
  dict_write_int32(iter, MESSAGE_KEY_ImageTransferId, transfer_id);
  prv_deliver(iter);
  return transfer_id;
}

// The cached message rows [from, to) as the UI's window: the initial load (mode initial) or an
// older or newer page, which replaces the window once complete. As the watchapp's phone side did.
static void prv_deliver_window_rows(StreamMode mode, bool silent, const RowRef *rows, int from,
                                    int to) {
  const int count = to - from;
  const int transfer_id = prv_start_stream(mode, silent, count);
  for (int i = from; i < to; i++) {
    int len;
    uint8_t *val = prv_read(&rows[i], &len);
    ChatDbMessage m;
    if (val && chat_db_parse_message(val, len, &m)) {
      prv_deliver_message("message", i - from, count, transfer_id, &m);
      prv_note_shown(&m);
    }
    app_free(val);
  }
  prv_deliver_done("messages_done", count, transfer_id);
}

// New messages [from, to) of the open chat, appended. total is every message known, which the UI
// takes as the most rows it may keep, so its window isn't cut down.
static void prv_deliver_appended(const RowRef *rows, int from, int to, int total) {
  const int transfer_id = prv_start_stream(ModeNewer, false, total);
  for (int i = from; i < to; i++) {
    int len;
    uint8_t *val = prv_read(&rows[i], &len);
    ChatDbMessage m;
    if (val && chat_db_parse_message(val, len, &m)) {
      prv_deliver_message("message_append", 0, total, transfer_id, &m);
      prv_note_shown(&m);
    }
    app_free(val);
  }
  prv_deliver_done("messages_done", total, transfer_id);
}

// Nothing older or newer to show.
static void prv_deliver_no_more(bool silent) {
  DictionaryIterator *iter = prv_begin("messages_done");
  dict_write_int32(iter, MESSAGE_KEY_Count, 0);
  if (silent) {
    dict_write_cstring(iter, MESSAGE_KEY_Text, "silent");
  }
  prv_deliver(iter);
}

// Index of the cached message with this id, -1 if none.
static int prv_find_message(const RowRef *rows, int count, const char *id) {
  for (int i = 0; id && i < count; i++) {
    int len;
    uint8_t *val = prv_read(&rows[i], &len);
    ChatDbMessage m;
    const bool match = val && chat_db_parse_message(val, len, &m) && prv_str_eq(m.id, id);
    app_free(val);
    if (match) {
      return i;
    }
  }
  return -1;
}

// First row of a window around the anchor with older_ahead rows before it, as the watchapp's
// phone side chose it (messageWindowAroundAnchor).
static int prv_window_start(int count, int anchor, int older_ahead) {
  int start = anchor < 0 ? count - WINDOW_ROWS : anchor - older_ahead;
  if (start + WINDOW_ROWS > count) {
    start = count - WINDOW_ROWS;
  }
  return start < 0 ? 0 : start;
}

static void prv_deliver_window(const uint8_t *chat_key) {
  RowRef rows[MAX_ROWS];
  const int count = prv_rows(CHAT_DB_KIND_MESSAGE, chat_key, rows, MAX_ROWS);
  s_ds.newest_shown = 0;
  prv_deliver_window_rows(ModeInitial, false, rows, prv_window_start(count, -1, 0), count);
}

// An older or newer page from the cache, around the selected message (anchor_id), past the
// window's edge message (edge_id). False if the cache has nothing past the edge.
static bool prv_deliver_page(const uint8_t *chat_key, StreamMode mode, bool silent,
                             const char *edge_id, const char *anchor_id) {
  RowRef rows[MAX_ROWS];
  const int count = prv_rows(CHAT_DB_KIND_MESSAGE, chat_key, rows, MAX_ROWS);
  const int edge = prv_find_message(rows, count, edge_id);
  const bool more = (mode == ModeOlder) ? edge > 0 : (edge >= 0 && edge < count - 1);
  if (!more) {
    return false;
  }
  const int start = prv_window_start(count, prv_find_message(rows, count, anchor_id), 4);
  const int end = start + WINDOW_ROWS < count ? start + WINDOW_ROWS : count;
  prv_deliver_window_rows(mode, silent, rows, start, end);
  return true;
}

static void prv_deliver_context(const uint8_t *chat_key, const char *id, bool full_text) {
  RowRef rows[MAX_ROWS];
  const int count = prv_rows(CHAT_DB_KIND_MESSAGE, chat_key, rows, MAX_ROWS);
  const int i = prv_find_message(rows, count, id);
  int len = 0;
  uint8_t *val = i >= 0 ? prv_read(&rows[i], &len) : nullptr;
  ChatDbMessage m;
  const bool found = val && chat_db_parse_message(val, len, &m);

  DictionaryIterator *iter = prv_begin("message_context");
  dict_write_cstring(iter, MESSAGE_KEY_MessageId, id ? id : "");
  if (!found) {
    dict_write_cstring(iter, MESSAGE_KEY_Sender, full_text ? "" : "Reply");
    dict_write_cstring(iter, MESSAGE_KEY_Text, "Message not loaded");
  } else if (full_text) {
    dict_write_cstring(iter, MESSAGE_KEY_Sender, "");
    prv_write_str(iter, MESSAGE_KEY_Text, m.text, MAX_TEXT_BYTES);
  } else {
    ChatDbStr sender = m.reply_sender.len ? m.reply_sender : (ChatDbStr){"Reply", 5};
    prv_write_str(iter, MESSAGE_KEY_Sender, sender, MAX_SENDER_BYTES);
    prv_write_str(iter, MESSAGE_KEY_Text, m.reply_text, MAX_TEXT_BYTES);
  }
  app_free(val);
  prv_deliver(iter);
}

////////////////////////////
// Commands from the UI
////////////////////////////

static const char *prv_tuple_str(DictionaryIterator *iter, uint32_t key) {
  Tuple *t = dict_find(iter, key);
  return (t && t->type == TUPLE_CSTRING) ? t->value->cstring : nullptr;
}

static bool prv_forward(const uint8_t *dict, size_t len) {
  return chat_service_send(dict, len);
}

static bool prv_avatar_asked(const uint8_t *key) {
  for (int i = 0; i < s_ds.avatar_asked_count; i++) {
    if (memcmp(s_ds.avatar_asked[i], key, CHAT_DB_CHAT_KEY_LEN) == 0) {
      return true;
    }
  }
  return false;
}

// Asks for the next queued picture, then schedules the one after. A send that fails (phone gone
// or the link backed up) drops the rest of the queue rather than blocking on each in turn.
static void prv_send_next_avatar(void *data) {
  s_ds.avatar_timer = nullptr;
  const int size = chat_ui_avatar_size();
  while (s_ds.avatar_queue_next < s_ds.avatar_queue_len) {
    const uint8_t *key = s_ds.avatar_queue[s_ds.avatar_queue_next++];
    char id[CHAT_KEY_HEX_LEN + 1];
    prv_hex(id, key, CHAT_DB_CHAT_KEY_LEN);
    if (size <= 0 || prv_avatar_asked(key) || chat_ui_has_avatar(id)) {
      continue;
    }
    char size_text[8];
    snprintf(size_text, sizeof(size_text), "%u", (uint8_t)size);
    uint8_t buf[96];
    DictionaryIterator iter;
    dict_write_begin(&iter, buf, sizeof(buf));
    dict_write_cstring(&iter, MESSAGE_KEY_Command, "get_avatar");
    dict_write_cstring(&iter, MESSAGE_KEY_ChatId, id);
    dict_write_cstring(&iter, MESSAGE_KEY_Text, size_text);
    if (!prv_forward(buf, dict_write_end(&iter))) {
      s_ds.avatar_queue_len = 0;
      return;
    }
    if (s_ds.avatar_asked_count < AVATAR_REQUESTS) {
      memcpy(s_ds.avatar_asked[s_ds.avatar_asked_count++], key, CHAT_DB_CHAT_KEY_LEN);
    }
    s_ds.avatar_timer = app_timer_register(AVATAR_GAP_MS, prv_send_next_avatar, nullptr);
    return;
  }
  s_ds.avatar_queue_len = 0;
}

// Chat pictures aren't kept in the chat db: the phone sends the newest chats' pictures on request,
// as the watchapp's phone side did (avatar_start / avatar / avatar_done).
static void prv_request_avatars(void) {
  if (chat_ui_avatar_size() <= 0 || !pbl_pp_session_get_system()) {
    return;
  }
  RowRef rows[MAX_CHATS];
  const int count = prv_rows(CHAT_DB_KIND_CHAT, nullptr, rows, MAX_CHATS);
  s_ds.avatar_queue_len = 0;
  s_ds.avatar_queue_next = 0;
  for (int i = count - 1; i >= 0 && s_ds.avatar_queue_len < AVATAR_REQUESTS; i--) {  // newest first
    if (!prv_avatar_asked(rows[i].key + 1)) {
      memcpy(s_ds.avatar_queue[s_ds.avatar_queue_len++], rows[i].key + 1, CHAT_DB_CHAT_KEY_LEN);
    }
  }
  if (s_ds.avatar_queue_len && !s_ds.avatar_timer) {
    s_ds.avatar_timer = app_timer_register(AVATAR_FIRST_DELAY_MS, prv_send_next_avatar, nullptr);
  }
}

static void prv_handle_command(const uint8_t *dict, size_t len) {
  DictionaryIterator iter;
  dict_read_begin_from_buffer(&iter, dict, len);
  const char *cmd = prv_tuple_str(&iter, MESSAGE_KEY_Command);
  if (!cmd) {
    return;
  }
  const char *text = prv_tuple_str(&iter, MESSAGE_KEY_Text);
  const char *reply_to = prv_tuple_str(&iter, MESSAGE_KEY_ReplyTo);
  const char *message_id = prv_tuple_str(&iter, MESSAGE_KEY_MessageId);
  uint8_t chat_key[CHAT_DB_CHAT_KEY_LEN];
  const bool has_chat = prv_unhex(chat_key, prv_tuple_str(&iter, MESSAGE_KEY_ChatId));
  const bool silent = text && strcmp(text, "silent") == 0;

  if (strcmp(cmd, "get_chats") == 0) {
    s_ds.chat_open = false;
    prv_deliver_settings();
    char hint_chat[CHAT_KEY_HEX_LEN + 1] = "";
    prv_deliver_chats(hint_chat);
    if (s_ds.launch_hint[0]) {
      s_ds.launch_hint[0] = '\0';
      chat_ui_open_chat(hint_chat);  // "" when no chat has that name: the UI shows the list
    }
    prv_request_avatars();
  } else if (strcmp(cmd, "get_messages") == 0 && has_chat) {
    s_ds.chat_open = true;
    memcpy(s_ds.open_chat, chat_key, sizeof(chat_key));
    prv_deliver_window(chat_key);
    prv_forward(dict, len); // the phone marks the chat read and refreshes it
  } else if (strcmp(cmd, "get_older_messages") == 0 && has_chat) {
    // The edge is the oldest message shown (ReplyTo); past the cache, the phone pages
    if (!prv_deliver_page(chat_key, ModeOlder, silent, reply_to, message_id) &&
        !prv_forward(dict, len)) {
      prv_deliver_no_more(silent);
    }
  } else if (strcmp(cmd, "get_newer_messages") == 0 && has_chat) {
    if (!prv_deliver_page(chat_key, ModeNewer, silent, reply_to, message_id)) {
      prv_deliver_no_more(silent);
    }
  } else if (strcmp(cmd, "get_message_text") == 0 && has_chat) {
    prv_deliver_context(chat_key, message_id, true);
  } else if (strcmp(cmd, "get_context") == 0 && has_chat) {
    prv_deliver_context(chat_key, message_id, false);
  } else if (strcmp(cmd, "leave_chat") == 0) {
    s_ds.chat_open = false;
  } else if (strcmp(cmd, "send_message") == 0 || strcmp(cmd, "edit_message") == 0 ||
             strcmp(cmd, "delete_message") == 0 || strcmp(cmd, "send_reaction") == 0) {
    if (!prv_forward(dict, len)) {
      const char *what = (cmd[0] == 's' && cmd[5] == 'm') ? "Send"
                         : (cmd[0] == 'e')                ? "Edit"
                         : (cmd[0] == 'd')                ? "Delete"
                                                          : "Reaction";
      char error[48];
      snprintf(error, sizeof(error), "%s failed: phone not connected", what);
      prv_deliver_simple("error", MESSAGE_KEY_Error, error);
    }
  }
  // Not needed without the watchapp's phone side: chat_first_paint, prefetch_*, save_draft,
  // get_image, cancel_image, start_qr.
}

typedef struct {
  size_t len;
  uint8_t dict[];
} Command;

static void prv_process_outbox(void *data) {
  Command *command = data;
  s_ds.out_busy = false;
  if (s_ds.outbox_sent) {
    DictionaryIterator iter;
    dict_read_begin_from_buffer(&iter, command->dict, command->len);
    s_ds.outbox_sent(&iter, nullptr);
  }
  prv_handle_command(command->dict, command->len);
  app_free(command);
}

////////////////////////////
// AppMessage shim
////////////////////////////

AppMessageResult chat_am_open(uint32_t size_inbound, uint32_t size_outbound) {
  return APP_MSG_OK;
}

AppMessageResult chat_am_outbox_begin(DictionaryIterator **iterator) {
  if (s_ds.out_busy) {
    return APP_MSG_BUSY;
  }
  dict_write_begin(&s_ds.out_iter, s_ds.out, sizeof(s_ds.out));
  *iterator = &s_ds.out_iter;
  return APP_MSG_OK;
}

AppMessageResult chat_am_outbox_send(void) {
  const uint32_t len = dict_write_end(&s_ds.out_iter);
  Command *command = app_malloc(sizeof(Command) + len);
  if (!command) {
    return APP_MSG_OUT_OF_MEMORY;
  }
  command->len = len;
  memcpy(command->dict, s_ds.out, len);
  s_ds.out_busy = true;
  // Like AppMessage, answer after the caller returns
  app_timer_register(0, prv_process_outbox, command);
  return APP_MSG_OK;
}

AppMessageInboxReceived chat_am_register_inbox_received(AppMessageInboxReceived cb) {
  AppMessageInboxReceived old = s_ds.inbox_received;
  s_ds.inbox_received = cb;
  return old;
}

AppMessageInboxDropped chat_am_register_inbox_dropped(AppMessageInboxDropped cb) {
  return nullptr; // nothing is ever dropped
}

AppMessageOutboxSent chat_am_register_outbox_sent(AppMessageOutboxSent cb) {
  AppMessageOutboxSent old = s_ds.outbox_sent;
  s_ds.outbox_sent = cb;
  return old;
}

AppMessageOutboxFailed chat_am_register_outbox_failed(AppMessageOutboxFailed cb) {
  AppMessageOutboxFailed old = s_ds.outbox_failed;
  s_ds.outbox_failed = cb;
  return old;
}

////////////////////////////
// Phone and chat db changes
////////////////////////////

static void prv_from_phone(const uint8_t *dict, size_t len) {
  if (!s_ds.inbox_received || len > sizeof(s_ds.in)) {
    return;
  }
  memcpy(s_ds.in, dict, len);
  DictionaryIterator iter;
  dict_read_begin_from_buffer(&iter, s_ds.in, len);
  s_ds.inbox_received(&iter, nullptr);
}

// The phone changed the chat db: show new messages of the open chat, or the new chat list.
static void prv_apply_changes(void *data) {
  s_ds.change_timer = nullptr;
  if (!s_ds.chat_open) {
    prv_deliver_chats(nullptr);
    return;
  }
  RowRef rows[MAX_ROWS];
  const int count = prv_rows(CHAT_DB_KIND_MESSAGE, s_ds.open_chat, rows, MAX_ROWS);
  int from = count;
  while (from > 0 && rows[from - 1].time > s_ds.newest_shown) {
    from--;
  }
  if (from < count) {
    prv_deliver_appended(rows, from, count, count);
  }
}

static void prv_blob_db_event(PebbleEvent *e, void *context) {
  if (e->blob_db.db_id != BlobDBIdChat) {
    return;
  }
  if (s_ds.change_timer) {
    app_timer_reschedule(s_ds.change_timer, CHANGE_DELAY_MS);
  } else {
    s_ds.change_timer = app_timer_register(CHANGE_DELAY_MS, prv_apply_changes, nullptr);
  }
}

bool chat_ds_has_launch_hint(void) {
  return s_ds.launch_hint[0] != '\0';
}

void chat_ds_init(void) {
  memset(&s_ds, 0, sizeof(s_ds));
  chat_service_take_launch_hint(s_ds.launch_hint, sizeof(s_ds.launch_hint));
  chat_service_set_handler(prv_from_phone);
  s_ds.blob_db_event = (EventServiceInfo){
    .type = PEBBLE_BLOBDB_EVENT,
    .handler = prv_blob_db_event,
  };
  event_service_client_subscribe(&s_ds.blob_db_event);
}

void chat_ds_deinit(void) {
  chat_service_set_handler(nullptr);
  event_service_client_unsubscribe(&s_ds.blob_db_event);
  if (s_ds.change_timer) {
    app_timer_cancel(s_ds.change_timer);
  }
  s_ds.change_timer = nullptr;
  if (s_ds.avatar_timer) {
    app_timer_cancel(s_ds.avatar_timer);
  }
  s_ds.avatar_timer = nullptr;
}
