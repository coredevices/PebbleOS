/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/blob_db/chat_db.h>
#include <pbl/settings/settings.h>
#include <pbl/util/units.h>

#define SETTINGS_FILE_NAME "/chatdb"
#define SETTINGS_FILE_SIZE (PBL_KIB(96))

static struct {
  struct pbl_settings_file settings_file;
  struct pbl_mutex mutex;
} s_chat_db;

//////////////////////
// Parsing
//////////////////////

typedef struct {
  const uint8_t *pos;
  const uint8_t *end;
  bool ok;
} Reader;

static uint32_t prv_read_uint(Reader *r, int size) {
  if (!r->ok || r->end - r->pos < size) {
    r->ok = false;
    return 0;
  }
  uint32_t v = 0;
  for (int i = 0; i < size; i++) {
    v |= (uint32_t)r->pos[i] << (8 * i);
  }
  r->pos += size;
  return v;
}

static ChatDbStr prv_read_str(Reader *r, int len_size) {
  const uint16_t len = prv_read_uint(r, len_size);
  if (!r->ok || r->end - r->pos < len) {
    r->ok = false;
    return (ChatDbStr){0};
  }
  ChatDbStr s = {(const char *)r->pos, len};
  r->pos += len;
  return s;
}

// A complete value: every field read and nothing left over.
static bool prv_done(const Reader *r) {
  return r->ok && r->pos == r->end;
}

bool chat_db_parse_chat(const uint8_t *val, int val_len, ChatDbChat *out) {
  Reader r = {val, val + val_len, val_len > 0};
  if (prv_read_uint(&r, 1) != CHAT_DB_VERSION) {
    return false;
  }
  out->flags = prv_read_uint(&r, 1);
  out->network = prv_read_uint(&r, 1);
  out->unread_count = prv_read_uint(&r, 2);
  out->last_activity = prv_read_uint(&r, 4);
  out->synced = prv_read_uint(&r, 4);
  out->title = prv_read_str(&r, 1);
  out->preview = prv_read_str(&r, 1);
  out->address = prv_read_str(&r, 1);
  return prv_done(&r);
}

bool chat_db_parse_features(const uint8_t *val, int val_len, ChatDbStr *out) {
  Reader r = {val, val + val_len, val_len > 0};
  if (prv_read_uint(&r, 1) != CHAT_DB_VERSION) {
    return false;
  }
  *out = prv_read_str(&r, 1);
  return prv_done(&r);
}

bool chat_db_parse_favorite(const uint8_t *val, int val_len, ChatDbFavorite *out) {
  Reader r = {val, val + val_len, val_len > 0};
  if (prv_read_uint(&r, 1) != CHAT_DB_VERSION) {
    return false;
  }
  out->network = prv_read_uint(&r, 1);
  out->name = prv_read_str(&r, 1);
  out->address = prv_read_str(&r, 1);
  return prv_done(&r);
}

bool chat_db_parse_meta(const uint8_t *val, int val_len, ChatDbMeta *out) {
  Reader r = {val, val + val_len, val_len > 0};
  if (prv_read_uint(&r, 1) != CHAT_DB_VERSION) {
    return false;
  }
  out->schema = prv_read_uint(&r, 1);
  out->epoch = prv_read_uint(&r, 4);
  return prv_done(&r);
}

bool chat_db_parse_message(const uint8_t *val, int val_len, ChatDbMessage *out) {
  Reader r = {val, val + val_len, val_len > 0};
  if (prv_read_uint(&r, 1) != CHAT_DB_VERSION) {
    return false;
  }
  out->flags = prv_read_uint(&r, 1);
  out->time = prv_read_uint(&r, 4);
  out->id = prv_read_str(&r, 1);
  out->sender = prv_read_str(&r, 1);
  out->text = prv_read_str(&r, 2);
  out->reactions = prv_read_str(&r, 1);
  out->reply_sender = prv_read_str(&r, 1);
  out->reply_text = prv_read_str(&r, 1);
  // Added later: a value without it is still a message
  out->status = r.ok && r.pos < r.end ? prv_read_str(&r, 1) : (ChatDbStr){0};
  if (r.ok && r.pos < r.end) {  // a photo
    out->image_token = prv_read_str(&r, 1);
    out->image_width = prv_read_uint(&r, 2);
    out->image_height = prv_read_uint(&r, 2);
  } else {
    out->image_token = (ChatDbStr){0};
    out->image_width = out->image_height = 0;
  }
  return prv_done(&r);
}

static bool prv_key_valid(const uint8_t *key, int key_len) {
  if (key_len < 1) {
    return false;
  }
  switch (key[0]) {
    case CHAT_DB_KIND_CHAT:
      return key_len == 1 + CHAT_DB_CHAT_KEY_LEN;
    case CHAT_DB_KIND_MESSAGE:
      return key_len == 1 + CHAT_DB_CHAT_KEY_LEN + CHAT_DB_MESSAGE_KEY_LEN;
    case CHAT_DB_KIND_FEATURES:
      return key_len == 2;
    case CHAT_DB_KIND_FAVORITE:
      return key_len == 1 + CHAT_DB_CHAT_KEY_LEN;
    case CHAT_DB_KIND_META:
      return key_len == 1;
    default:
      return false;
  }
}

static bool prv_record_valid(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  if (!prv_key_valid(key, key_len)) {
    return false;
  }
  switch (key[0]) {
    case CHAT_DB_KIND_CHAT: {
      ChatDbChat chat;
      return chat_db_parse_chat(val, val_len, &chat);
    }
    case CHAT_DB_KIND_MESSAGE: {
      ChatDbMessage message;
      return chat_db_parse_message(val, val_len, &message);
    }
    case CHAT_DB_KIND_FEATURES: {
      ChatDbStr features;
      return chat_db_parse_features(val, val_len, &features);
    }
    case CHAT_DB_KIND_FAVORITE: {
      ChatDbFavorite favorite;
      return chat_db_parse_favorite(val, val_len, &favorite);
    }
    default: {
      ChatDbMeta meta;
      return chat_db_parse_meta(val, val_len, &meta);
    }
  }
}

//////////////////////
// Settings helpers
//////////////////////

static int prv_lock_mutex_and_open_file(void) {
  pbl_mutex_lock(&s_chat_db.mutex, PBL_FOREVER);
  int rv = pbl_settings_open_growable(&s_chat_db.settings_file, SETTINGS_FILE_NAME,
                                      SETTINGS_FILE_SIZE, PBL_KIB(4));
  if (rv != 0) {
    pbl_mutex_unlock(&s_chat_db.mutex);
  }
  return rv;
}

static void prv_close_file_and_unlock_mutex(void) {
  pbl_settings_close(&s_chat_db.settings_file);
  pbl_mutex_unlock(&s_chat_db.mutex);
}

typedef struct {
  ChatDbEachCb cb;
  void *context;
} EachContext;

static bool prv_each(struct pbl_settings_file *file, struct pbl_settings_record *info,
                     void *context) {
  EachContext *each = context;
  if (info->key_len == 0 || info->val_len == 0) {
    return true;
  }
  uint8_t *buf = pbl_malloc(info->key_len + info->val_len);
  if (!buf) {
    return false;
  }
  pbl_settings_read_key(file, buf, info->key_len);
  pbl_settings_read_val(file, buf + info->key_len, info->val_len);
  const bool more =
      each->cb(buf, info->key_len, buf + info->key_len, info->val_len, each->context);
  pbl_free(buf);
  return more;
}

void chat_db_each(ChatDbEachCb cb, void *context) {
  if (prv_lock_mutex_and_open_file() != 0) {
    return;
  }
  EachContext each = {cb, context};
  pbl_settings_each(&s_chat_db.settings_file, prv_each, &each);
  prv_close_file_and_unlock_mutex();
}

/////////////////////////
// Blob DB API
/////////////////////////

void chat_db_init(void) {
  memset(&s_chat_db, 0, sizeof(s_chat_db));
  pbl_mutex_init(&s_chat_db.mutex);
}

int chat_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  if (!prv_record_valid(key, key_len, val, val_len)) {
    return -EINVAL;
  }
  int rv = prv_lock_mutex_and_open_file();
  if (rv != 0) {
    return rv;
  }
  rv = pbl_settings_set(&s_chat_db.settings_file, key, key_len, val, val_len);
  prv_close_file_and_unlock_mutex();
  return rv;
}

int chat_db_get_len(const uint8_t *key, int key_len) {
  if (!prv_key_valid(key, key_len)) {
    return 0;
  }
  int rv = prv_lock_mutex_and_open_file();
  if (rv != 0) {
    return rv;
  }
  rv = pbl_settings_get_len(&s_chat_db.settings_file, key, key_len);
  prv_close_file_and_unlock_mutex();
  return rv;
}

int chat_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len) {
  if (!prv_key_valid(key, key_len) || val_out == nullptr) {
    return -EINVAL;
  }
  int rv = prv_lock_mutex_and_open_file();
  if (rv != 0) {
    return rv;
  }
  rv = pbl_settings_get(&s_chat_db.settings_file, key, key_len, val_out, val_out_len);
  prv_close_file_and_unlock_mutex();
  return rv;
}

int chat_db_delete(const uint8_t *key, int key_len) {
  if (!prv_key_valid(key, key_len)) {
    return -EINVAL;
  }
  int rv = prv_lock_mutex_and_open_file();
  if (rv != 0) {
    return rv;
  }
  rv = pbl_settings_delete(&s_chat_db.settings_file, key, key_len);
  prv_close_file_and_unlock_mutex();
  return rv;
}

int chat_db_flush(void) {
  pbl_mutex_lock(&s_chat_db.mutex, PBL_FOREVER);
  int rv = pbl_fs_unlink(SETTINGS_FILE_NAME);
  pbl_mutex_unlock(&s_chat_db.mutex);
  return rv;
}

int chat_db_compact(void) {
  int rv = prv_lock_mutex_and_open_file();
  if (rv != 0) {
    return rv;
  }
  rv = pbl_settings_compact(&s_chat_db.settings_file);
  prv_close_file_and_unlock_mutex();
  return rv;
}
