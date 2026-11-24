/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup services_blob_db_chat_db Chat database
 * @ingroup services_blob_db
 * @brief Recent chats and their newest messages (::BlobDBIdChat), written by the phone.
 *
 * Keys start with a record kind:
 * - ``'C'`` + 8 byte chat key: a chat.
 * - ``'M'`` + 8 byte chat key + 8 byte message key: a message of that chat.
 * - ``'F'``: what the phone can do, a comma separated feature list.
 *
 * Values are little endian. ``str8`` is a length byte and UTF-8 bytes, ``str16`` a 16 bit length
 * and UTF-8 bytes, without NUL.
 * - Chat: version (1), flags (::ChatDbChatFlags), unread count (u16), last activity (u32 UTC),
 *   last synced (u32 UTC), title (str8), preview (str8).
 * - Message: version (1), flags (::ChatDbMessageFlags), time (u32 UTC), id (str8), sender (str8),
 *   text (str16), reactions (str8), reply sender (str8), reply text (str8), and optionally
 *   status (str8), then optionally a photo: image token (str8), width (u16), height (u16).
 * - Features: str8.
 * @{
 */

/** @brief Length of a chat key. */
#define CHAT_DB_CHAT_KEY_LEN 8
/** @brief Length of a message key. */
#define CHAT_DB_MESSAGE_KEY_LEN 8
/** @brief Key kind of a chat record. */
#define CHAT_DB_KIND_CHAT 'C'
/** @brief Key kind of a message record. */
#define CHAT_DB_KIND_MESSAGE 'M'
/** @brief Key kind of the features record. */
#define CHAT_DB_KIND_FEATURES 'F'
/** @brief Version of chat and message values. */
#define CHAT_DB_VERSION 1

/** @brief Chat flags. */
typedef enum {
  /** A group, not a direct chat. */
  ChatDbChatFlagGroup = 1 << 0,
  /** Muted. */
  ChatDbChatFlagMuted = 1 << 1,
} ChatDbChatFlags;

/** @brief Message flags. */
typedef enum {
  /** Sent by the user. */
  ChatDbMessageFlagOutgoing = 1 << 0,
} ChatDbMessageFlags;

/** @brief A string inside a record value, not NUL terminated. */
typedef struct {
  /** First byte. */
  const char *str;
  /** Length in bytes. */
  uint16_t len;
} ChatDbStr;

/** @brief A parsed chat value; strings point into the value. */
typedef struct {
  /** ::ChatDbChatFlags. */
  uint8_t flags;
  /** Unread messages. */
  uint16_t unread_count;
  /** Time of the newest message, UTC. */
  uint32_t last_activity;
  /** When the phone last synced this chat, UTC. */
  uint32_t synced;
  /** Chat name. */
  ChatDbStr title;
  /** Newest message, shortened. */
  ChatDbStr preview;
} ChatDbChat;

/** @brief A parsed message value; strings point into the value. */
typedef struct {
  /** ::ChatDbMessageFlags. */
  uint8_t flags;
  /** Time sent, UTC. */
  uint32_t time;
  /** The phone's message id. */
  ChatDbStr id;
  /** Sender name. */
  ChatDbStr sender;
  /** Text. */
  ChatDbStr text;
  /** Reactions summary. */
  ChatDbStr reactions;
  /** Sender of the message replied to, empty if none. */
  ChatDbStr reply_sender;
  /** Text of the message replied to, empty if none. */
  ChatDbStr reply_text;
  /** Where my message got to, as shown under it ("Sent", "Seen at 10:52"); empty for others' and older ones. */
  ChatDbStr status;
  /** A photo's token for get_image, empty when the message has none. */
  ChatDbStr image_token;
  /** The photo's size, 0 when unknown. */
  uint16_t image_width;
  uint16_t image_height;
} ChatDbMessage;

/**
 * @brief Parse a chat value.
 *
 * @param val Value.
 * @param val_len Length of @p val in bytes.
 * @param[out] out Parsed chat.
 * @return true if the value is a complete version 1 chat.
 */
bool chat_db_parse_chat(const uint8_t *val, int val_len, ChatDbChat *out);

/**
 * @brief Parse a message value.
 *
 * @param val Value.
 * @param val_len Length of @p val in bytes.
 * @param[out] out Parsed message.
 * @return true if the value is a complete version 1 message.
 */
bool chat_db_parse_message(const uint8_t *val, int val_len, ChatDbMessage *out);

/**
 * @brief Callback for chat_db_each().
 *
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @param val Value, valid only during the call.
 * @param val_len Length of @p val in bytes.
 * @param context Context passed to chat_db_each().
 * @return true to continue, false to stop.
 */
typedef bool (*ChatDbEachCb)(const uint8_t *key, int key_len, const uint8_t *val, int val_len,
                             void *context);

/**
 * @brief Call @p cb for every record. The database is locked meanwhile; @p cb must not write it.
 *
 * @param cb Callback.
 * @param context Passed to @p cb.
 */
void chat_db_each(ChatDbEachCb cb, void *context);

/** @brief Initialize the chat database. */
void chat_db_init(void);

/**
 * @brief Insert or replace a record. Records that don't parse are refused.
 *
 * @return 0 on success, a negative errno otherwise.
 */
int chat_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record.
 *
 * @return Length of the value in bytes, 0 if not found, or a negative errno.
 */
int chat_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record.
 *
 * @return 0 on success, a negative errno otherwise.
 */
int chat_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record.
 *
 * @return 0 on success, a negative errno otherwise.
 */
int chat_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete every record.
 *
 * @return 0 on success, a negative errno otherwise.
 */
int chat_db_flush(void);

/**
 * @brief Compact the settings file backing the database.
 *
 * @return 0 on success, a negative errno otherwise.
 */
int chat_db_compact(void);

/** @} */
