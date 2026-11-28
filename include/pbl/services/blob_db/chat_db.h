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
 * - ``'F'`` + network id (1 byte, ::ChatDbNetwork): what the phone can do on that network.
 * - ``'P'`` + 8 byte chat key: a favourite, the chat to send to.
 * - ``'H'``: the presence hold: while the user is at a computer, notifications from these iOS
 *   apps aren't shown (the phone's Android notifications are held on the phone).
 * - ``'V'``: the db's meta record; the Chat app asks the phone to wipe and resend the db when it
 *   is missing or its schema is not ::CHAT_DB_VERSION.
 *
 * Values are little endian. ``str8`` is a length byte and UTF-8 bytes, ``str16`` a 16 bit length
 * and UTF-8 bytes, without NUL.
 * - Chat: version, flags (::ChatDbChatFlags), network (u8), unread count (u16), last activity
 *   (u32 UTC), last synced (u32 UTC), title (str8), preview (str8), address (str8, may be empty).
 * - Message: version, flags (::ChatDbMessageFlags), time (u32 UTC), id (str8), sender (str8),
 *   text (str16), reactions (str8), reply sender (str8), reply text (str8), and optionally
 *   status (str8), then optionally a photo: image token (str8), width (u16), height (u16).
 * - Features: version, comma separated list (str8).
 * - Favourite: version, network (u8), name (str8), address (str8).
 * - Meta: version, schema (u8), epoch (u32, how many times the phone reset the db).
 * - Hold: version, until (u32 UTC, the lease the computer renews: a stale hold lapses by itself),
 *   iOS app ids (str8, comma separated).
 *
 * Every value starts with ::CHAT_DB_VERSION; other versions are refused. The db is a cache of the
 * phone's: a format change bumps the version and the phone resends everything.
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
/** @brief Key kind of a network's features record. */
#define CHAT_DB_KIND_FEATURES 'F'
/** @brief Key kind of a favourite. */
#define CHAT_DB_KIND_FAVORITE 'P'
/** @brief Key kind of the presence hold. */
#define CHAT_DB_KIND_HOLD 'H'
/** @brief Key kind of the meta record. */
#define CHAT_DB_KIND_META 'V'
/** @brief Version of every value, and the schema the meta record must name. */
#define CHAT_DB_VERSION 2

/** @brief Where a chat comes from. */
typedef enum {
  /** Beeper (gomuks or the Beeper app). */
  ChatDbNetworkBeeper = 0,
  /** The phone's own SMS. */
  ChatDbNetworkSms = 1,
  /** Number of networks this firmware knows. */
  ChatDbNetworkCount,
} ChatDbNetwork;

/** @brief Chat flags. */
typedef enum {
  /** A group, not a direct chat. */
  ChatDbChatFlagGroup = 1 << 0,
  /** Muted. */
  ChatDbChatFlagMuted = 1 << 1,
  /** A search result: a person with no conversation yet. */
  ChatDbChatFlagContact = 1 << 2,
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
  /** ::ChatDbNetwork. */
  uint8_t network;
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
  /** The other side's address (an SMS number), empty when the network has none. */
  ChatDbStr address;
} ChatDbChat;

/** @brief A parsed favourite; strings point into the value. */
typedef struct {
  /** ::ChatDbNetwork. */
  uint8_t network;
  /** Name to show. */
  ChatDbStr name;
  /** Address, may be empty. */
  ChatDbStr address;
  /** Place among the favourites, 0 first (optional trailing byte, 0 when absent). */
  uint8_t order;
} ChatDbFavorite;

/** @brief The parsed meta record. */
typedef struct {
  /** Schema the phone wrote. */
  uint8_t schema;
  /** How many times the phone reset the db. */
  uint32_t epoch;
} ChatDbMeta;

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
 * @return true if the value is a complete chat of ::CHAT_DB_VERSION.
 */
bool chat_db_parse_chat(const uint8_t *val, int val_len, ChatDbChat *out);

/** @brief Parse a features value into its comma separated list. */
bool chat_db_parse_features(const uint8_t *val, int val_len, ChatDbStr *out);

/** @brief Parse a favourite value. */
bool chat_db_parse_favorite(const uint8_t *val, int val_len, ChatDbFavorite *out);

/** @brief Parse the meta value. */
bool chat_db_parse_meta(const uint8_t *val, int val_len, ChatDbMeta *out);

/**
 * @brief Parse a message value.
 *
 * @param val Value.
 * @param val_len Length of @p val in bytes.
 * @param[out] out Parsed message.
 * @return true if the value is a complete message of ::CHAT_DB_VERSION.
 */
bool chat_db_parse_message(const uint8_t *val, int val_len, ChatDbMessage *out);

/**
 * @brief Whether a notification's body shows this message text: the same text, the text after a
 * sender ("Alice: hi"), or the start of a longer text the notification cut short (also after a
 * sender). The whole body is tried, then its last line (a notification can list several
 * messages).
 *
 * @param text Message text.
 * @param body Notification body.
 */
bool chat_db_text_in_notification(ChatDbStr text, const char *body);

/** @brief The parsed presence hold. */
typedef struct {
  /** Until when (UTC). */
  uint32_t until;
  /** iOS app ids, comma separated. */
  ChatDbStr apps;
} ChatDbHold;

/** @brief Parse the presence hold value. */
bool chat_db_parse_hold(const uint8_t *val, int val_len, ChatDbHold *out);

/**
 * @brief Whether a notification from this iOS app is held now: the user is at a computer.
 *
 * @param app_id iOS app id, not NUL terminated.
 * @param app_id_len Its length.
 * @param now UTC.
 */
bool chat_db_is_held(const char *app_id, size_t app_id_len, uint32_t now);

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
