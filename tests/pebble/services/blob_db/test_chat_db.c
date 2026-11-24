/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/services/blob_db/chat_db.h>

#include <clar.h>

#include <fake_kernel_services_notifications.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>
#include <fake_sys_work.h>

static const uint8_t s_chat_key[] = {'C', 1, 2, 3, 4, 5, 6, 7, 8};
static const uint8_t s_chat[] = {
  2,          // version
  0x01,       // flags: group
  0,          // network: Beeper
  3,    0,    // unread
  0x10, 0, 0, 0, // last activity
  0x20, 0, 0, 0, // synced
  4,    'T', 'a', 'z', '!', // title
  2,    'h', 'i', // preview
  0,    // address
};

static const uint8_t s_message_key[] = {'M', 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9, 9, 9, 9, 9, 9};
static const uint8_t s_message[] = {
  2,             // version
  0x01,          // flags: outgoing
  0x30, 0, 0, 0, // time
  2,    '4', '2', // id
  3,    'A', 'n', 'n', // sender
  5,    0,   'h', 'e', 'l', 'l', 'o', // text
  0,    // reactions
  0,    // reply sender
  0,    // reply text
};

static const uint8_t s_features_key[] = {'F', 0};
static const uint8_t s_features[] = {2, 5, 'r', 'e', 'a', 'c', 't'};

// The spec's shared test vectors (docs/superpowers/specs/2026-11-24-chat-networks-design.md in
// pebble-apps); CoreApp's codec test writes the same bytes.
static const uint8_t s_v_key[] = {'V'};
static const uint8_t s_v[] = {0x02, 0x02, 0x07, 0x00, 0x00, 0x00};
static const uint8_t s_sms_features_key[] = {'F', 1};
static const uint8_t s_sms_features[] = {0x02, 0x0d, 's', 'e', 'a', 'r', 'c', 'h', '_',
                                         'p', 'e', 'o', 'p', 'l', 'e'};
static const uint8_t s_sms_chat[] = {0x02, 0x00, 0x01, 0x03, 0x00, 0x00, 0xe1, 0xf5, 0x05,
                                     0x00, 0xe1, 0xf5, 0x05, 0x03, 'A', 'n', 'n', 0x02, 'h', 'i',
                                     0x0c, '+', '1', '4', '1', '5', '5', '5', '5', '0', '1', '2',
                                     '3'};
static const uint8_t s_fav_key[] = {'P', 1, 2, 3, 4, 5, 6, 7, 8};
static const uint8_t s_fav[] = {0x02, 0x01, 0x03, 'M', 'o', 'm', 0x0c, '+', '1', '4', '1',
                                '5', '5', '5', '5', '0', '1', '2', '3'};

void test_chat_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  chat_db_init();
}

void test_chat_db__cleanup(void) {}

void test_chat_db__insert_and_read_chat(void) {
  cl_assert_equal_i(chat_db_insert(s_chat_key, sizeof(s_chat_key), s_chat, sizeof(s_chat)), 0);
  cl_assert_equal_i(chat_db_get_len(s_chat_key, sizeof(s_chat_key)), sizeof(s_chat));
  uint8_t buf[sizeof(s_chat)];
  cl_assert_equal_i(chat_db_read(s_chat_key, sizeof(s_chat_key), buf, sizeof(buf)), 0);
  ChatDbChat chat;
  cl_assert(chat_db_parse_chat(buf, sizeof(buf), &chat));
  cl_assert_equal_i(chat.flags, ChatDbChatFlagGroup);
  cl_assert_equal_i(chat.unread_count, 3);
  cl_assert_equal_i(chat.last_activity, 0x10);
  cl_assert_equal_i(chat.synced, 0x20);
  cl_assert_equal_i(chat.title.len, 4);
  cl_assert(memcmp(chat.title.str, "Taz!", 4) == 0);
  cl_assert_equal_i(chat.preview.len, 2);
}

void test_chat_db__insert_and_parse_message(void) {
  cl_assert_equal_i(
      chat_db_insert(s_message_key, sizeof(s_message_key), s_message, sizeof(s_message)), 0);
  ChatDbMessage m;
  cl_assert(chat_db_parse_message(s_message, sizeof(s_message), &m));
  cl_assert_equal_i(m.flags, ChatDbMessageFlagOutgoing);
  cl_assert_equal_i(m.time, 0x30);
  cl_assert_equal_i(m.id.len, 2);
  cl_assert_equal_i(m.text.len, 5);
  cl_assert(memcmp(m.text.str, "hello", 5) == 0);
  cl_assert_equal_i(m.reply_text.len, 0);
}

void test_chat_db__features(void) {
  cl_assert_equal_i(
      chat_db_insert(s_features_key, sizeof(s_features_key), s_features, sizeof(s_features)), 0);
  cl_assert_equal_i(
      chat_db_insert(s_features_key, sizeof(s_features_key), s_features, sizeof(s_features) - 1),
      -EINVAL);
}

void test_chat_db__rejects_bad_records(void) {
  const uint8_t unknown_kind[] = {'X', 1, 2, 3, 4, 5, 6, 7, 8};
  cl_assert_equal_i(chat_db_insert(unknown_kind, sizeof(unknown_kind), s_chat, sizeof(s_chat)),
                    -EINVAL);
  cl_assert_equal_i(chat_db_insert(s_chat_key, sizeof(s_chat_key) - 1, s_chat, sizeof(s_chat)),
                    -EINVAL);
  // Truncated: the preview's length byte claims more than is there.
  cl_assert_equal_i(chat_db_insert(s_chat_key, sizeof(s_chat_key), s_chat, sizeof(s_chat) - 1),
                    -EINVAL);
  uint8_t wrong_version[sizeof(s_chat)];
  memcpy(wrong_version, s_chat, sizeof(s_chat));
  wrong_version[0] = 1;  // v1 rows are refused: the phone wipes and resends them
  cl_assert_equal_i(
      chat_db_insert(s_chat_key, sizeof(s_chat_key), wrong_version, sizeof(wrong_version)),
      -EINVAL);
  // A chat value under a message key.
  cl_assert_equal_i(chat_db_insert(s_message_key, sizeof(s_message_key), s_chat, sizeof(s_chat)),
                    -EINVAL);
  cl_assert_equal_i(
      chat_db_insert(s_message_key, sizeof(s_message_key), s_message, sizeof(s_message) - 1),
      -EINVAL);
}

static bool prv_count(const uint8_t *key, int key_len, const uint8_t *val, int val_len,
                      void *context) {
  (*(int *)context)++;
  return true;
}

void test_chat_db__each_and_flush(void) {
  chat_db_insert(s_chat_key, sizeof(s_chat_key), s_chat, sizeof(s_chat));
  chat_db_insert(s_message_key, sizeof(s_message_key), s_message, sizeof(s_message));
  int count = 0;
  chat_db_each(prv_count, &count);
  cl_assert_equal_i(count, 2);

  cl_assert_equal_i(chat_db_delete(s_chat_key, sizeof(s_chat_key)), 0);
  count = 0;
  chat_db_each(prv_count, &count);
  cl_assert_equal_i(count, 1);

  chat_db_flush();
  count = 0;
  chat_db_each(prv_count, &count);
  cl_assert_equal_i(count, 0);
}

void test_chat_db__v2_vectors(void) {
  cl_assert_equal_i(chat_db_insert(s_v_key, sizeof(s_v_key), s_v, sizeof(s_v)), 0);
  ChatDbMeta meta;
  cl_assert(chat_db_parse_meta(s_v, sizeof(s_v), &meta));
  cl_assert_equal_i(meta.schema, CHAT_DB_VERSION);
  cl_assert_equal_i(meta.epoch, 7);

  cl_assert_equal_i(chat_db_insert(s_sms_features_key, sizeof(s_sms_features_key),
                                   s_sms_features, sizeof(s_sms_features)), 0);
  ChatDbStr features;
  cl_assert(chat_db_parse_features(s_sms_features, sizeof(s_sms_features), &features));
  cl_assert_equal_i(features.len, 13);
  cl_assert(memcmp(features.str, "search_people", 13) == 0);

  cl_assert_equal_i(chat_db_insert(s_chat_key, sizeof(s_chat_key), s_sms_chat,
                                   sizeof(s_sms_chat)), 0);
  ChatDbChat chat;
  cl_assert(chat_db_parse_chat(s_sms_chat, sizeof(s_sms_chat), &chat));
  cl_assert_equal_i(chat.network, ChatDbNetworkSms);
  cl_assert_equal_i(chat.unread_count, 3);
  cl_assert_equal_i(chat.last_activity, 100000000);
  cl_assert_equal_i(chat.address.len, 12);
  cl_assert(memcmp(chat.address.str, "+14155550123", 12) == 0);

  cl_assert_equal_i(chat_db_insert(s_fav_key, sizeof(s_fav_key), s_fav, sizeof(s_fav)), 0);
  ChatDbFavorite fav;
  cl_assert(chat_db_parse_favorite(s_fav, sizeof(s_fav), &fav));
  cl_assert_equal_i(fav.network, ChatDbNetworkSms);
  cl_assert_equal_i(fav.name.len, 3);
  cl_assert(memcmp(fav.name.str, "Mom", 3) == 0);
  cl_assert_equal_i(fav.address.len, 12);
}

void test_chat_db__v2_rejects_bad_keys(void) {
  const uint8_t bare_features_key[] = {'F'};
  cl_assert_equal_i(chat_db_insert(bare_features_key, sizeof(bare_features_key), s_features,
                                   sizeof(s_features)), -EINVAL);
  const uint8_t long_v_key[] = {'V', 0};
  cl_assert_equal_i(chat_db_insert(long_v_key, sizeof(long_v_key), s_v, sizeof(s_v)), -EINVAL);
  cl_assert_equal_i(chat_db_insert(s_fav_key, sizeof(s_fav_key), s_fav, sizeof(s_fav) - 1),
                    -EINVAL);
}
