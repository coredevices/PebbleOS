/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/clients/ams.h>
#include <pbl/util/size.h>

#include <clar.h>
#include <fake_gattc_service.h>

enum {
  CHR_REMOTE_COMMAND,
  CHR_ENTITY_UPDATE,
  CHR_ENTITY_ATTRIBUTE,
};

PBL_BT_AMS_CLIENT_DEFINE(s_client);

static uint8_t s_written_chr;
static uint8_t s_written[16];
static uint16_t s_written_len;
static int s_num_writes;
static int s_write_rc;

static int s_num_discovered;
static int s_num_subscribed;
static enum pbl_bt_ams_source s_source;
static int s_num_commands_cb;
static uint8_t s_commands[16];
static uint8_t s_num_commands;
static int s_num_updates;
static struct pbl_bt_ams_update s_update;
static char s_value[32];
static int s_num_commands_sent;
static int s_num_registered;
static int s_num_attrs_read;
static enum pbl_bt_ams_entity s_entity;
static uint8_t s_attr;
static uint16_t s_len;
static int s_status;

static int prv_handle_write(uint8_t chr, const uint8_t *data, uint16_t len) {
  if (s_write_rc != 0) {
    return s_write_rc;
  }
  cl_assert(len <= sizeof(s_written));
  s_written_chr = chr;
  memcpy(s_written, data, len);
  s_written_len = len;
  s_num_writes++;
  return 0;
}

static void prv_discovered(struct pbl_bt_ams_client *client, int status) {
  cl_assert(client == &s_client);
  s_num_discovered++;
  s_status = status;
}

static void prv_subscribed(struct pbl_bt_ams_client *client, enum pbl_bt_ams_source source,
                           bool enable, int status) {
  cl_assert(enable);
  s_num_subscribed++;
  s_source = source;
  s_status = status;
}

static void prv_commands(struct pbl_bt_ams_client *client, const uint8_t *commands,
                         uint8_t num_commands) {
  cl_assert(num_commands <= sizeof(s_commands));
  s_num_commands_cb++;
  memcpy(s_commands, commands, num_commands);
  s_num_commands = num_commands;
}

static void prv_update(struct pbl_bt_ams_client *client, const struct pbl_bt_ams_update *update) {
  cl_assert(update->len < sizeof(s_value));
  s_num_updates++;
  s_update = *update;
  memcpy(s_value, update->value, update->len);
  s_value[update->len] = '\0';
}

static void prv_command_sent(struct pbl_bt_ams_client *client, int status) {
  s_num_commands_sent++;
  s_status = status;
}

static void prv_registered(struct pbl_bt_ams_client *client, int status) {
  s_num_registered++;
  s_status = status;
}

static void prv_attr_read(struct pbl_bt_ams_client *client, int status,
                          enum pbl_bt_ams_entity entity, uint8_t attr, const uint8_t *value,
                          uint16_t len) {
  cl_assert(len < sizeof(s_value));
  s_num_attrs_read++;
  s_status = status;
  s_entity = entity;
  s_attr = attr;
  s_len = len;
  if (status == 0) {
    memcpy(s_value, value, len);
    s_value[len] = '\0';
  } else {
    cl_assert(value == nullptr);
  }
}

static const struct pbl_bt_ams_client_cb s_cb = {
  .discovered = prv_discovered,
  .subscribed = prv_subscribed,
  .commands = prv_commands,
  .update = prv_update,
  .command_sent = prv_command_sent,
  .registered = prv_registered,
  .attr_read = prv_attr_read,
};

void test_ams_client__initialize(void) {
  fake_gattc_service_init();
  fake_gattc_service_set_write_handler(prv_handle_write);
  s_num_writes = 0;
  s_write_rc = 0;
  s_num_discovered = 0;
  s_num_subscribed = 0;
  s_num_commands_cb = 0;
  s_num_updates = 0;
  s_num_commands_sent = 0;
  s_num_registered = 0;
  s_num_attrs_read = 0;
  s_status = -1;
  cl_assert_equal_i(pbl_bt_ams_client_start(&s_client, 1, &s_cb), 0);
}

void test_ams_client__cleanup(void) {
  pbl_bt_ams_client_stop(&s_client);
}

void test_ams_client__start_twice(void) {
  cl_assert_equal_i(pbl_bt_ams_client_start(&s_client, 1, &s_cb), -EALREADY);
}

void test_ams_client__discovered_and_subscribed(void) {
  fake_gattc_service_discovered(0);
  cl_assert_equal_i(s_num_discovered, 1);
  cl_assert_equal_i(s_status, 0);

  cl_assert_equal_i(pbl_bt_ams_client_subscribe(&s_client, PBL_BT_AMS_SOURCE_ENTITY_UPDATE, true),
                    0);
  fake_gattc_service_subscribed(CHR_ENTITY_UPDATE, true, 0);
  cl_assert_equal_i(s_num_subscribed, 1);
  cl_assert_equal_i(s_source, PBL_BT_AMS_SOURCE_ENTITY_UPDATE);

  fake_gattc_service_subscribed(CHR_REMOTE_COMMAND, true, 3);
  cl_assert_equal_i(s_num_subscribed, 2);
  cl_assert_equal_i(s_source, PBL_BT_AMS_SOURCE_REMOTE_COMMAND);
  cl_assert_equal_i(s_status, 3);
}

void test_ams_client__not_found(void) {
  fake_gattc_service_discovered(-ENOENT);
  cl_assert_equal_i(s_num_discovered, 1);
  cl_assert_equal_i(s_status, -ENOENT);
}

void test_ams_client__send_command(void) {
  static const uint8_t cmd[] = {0x05};

  cl_assert_equal_i(pbl_bt_ams_client_send_command(&s_client, PBL_BT_AMS_COMMAND_VOLUME_UP), 0);
  cl_assert_equal_i(s_num_writes, 1);
  cl_assert_equal_i(s_written_chr, CHR_REMOTE_COMMAND);
  cl_assert_equal_i(s_written_len, sizeof(cmd));
  cl_assert_equal_m(s_written, cmd, sizeof(cmd));

  fake_gattc_service_written(CHR_REMOTE_COMMAND, PBL_BT_AMS_ERR_INVALID_COMMAND);
  cl_assert_equal_i(s_num_commands_sent, 1);
  cl_assert_equal_i(s_status, PBL_BT_AMS_ERR_INVALID_COMMAND);
  cl_assert_equal_i(s_num_registered, 0);
}

void test_ams_client__send_command_refused(void) {
  s_write_rc = -ENOMEM;
  cl_assert_equal_i(pbl_bt_ams_client_send_command(&s_client, PBL_BT_AMS_COMMAND_PLAY), -ENOMEM);
  cl_assert_equal_i(s_num_commands_sent, 0);
}

void test_ams_client__commands(void) {
  static const uint8_t commands[] = {PBL_BT_AMS_COMMAND_PLAY, PBL_BT_AMS_COMMAND_NEXT_TRACK};

  fake_gattc_service_notified(CHR_REMOTE_COMMAND, commands, sizeof(commands));
  cl_assert_equal_i(s_num_commands_cb, 1);
  cl_assert_equal_i(s_num_commands, sizeof(commands));
  cl_assert_equal_m(s_commands, commands, sizeof(commands));

  /* None: nothing is playing. */
  fake_gattc_service_notified(CHR_REMOTE_COMMAND, commands, 0);
  cl_assert_equal_i(s_num_commands_cb, 2);
  cl_assert_equal_i(s_num_commands, 0);
}

void test_ams_client__register(void) {
  static const uint8_t attrs[] = {PBL_BT_AMS_TRACK_ATTR_ARTIST, PBL_BT_AMS_TRACK_ATTR_TITLE};
  static const uint8_t cmd[] = {0x02, 0x00, 0x02};

  cl_assert_equal_i(
      pbl_bt_ams_client_register(&s_client, PBL_BT_AMS_ENTITY_TRACK, attrs, PBL_ARRAY_SIZE(attrs)),
      0);
  cl_assert_equal_i(s_num_writes, 1);
  cl_assert_equal_i(s_written_chr, CHR_ENTITY_UPDATE);
  cl_assert_equal_i(s_written_len, sizeof(cmd));
  cl_assert_equal_m(s_written, cmd, sizeof(cmd));

  fake_gattc_service_written(CHR_ENTITY_UPDATE, 0);
  cl_assert_equal_i(s_num_registered, 1);
  cl_assert_equal_i(s_status, 0);
  cl_assert_equal_i(s_num_commands_sent, 0);
}

void test_ams_client__register_too_many(void) {
  static const uint8_t attrs[PBL_BT_AMS_CLIENT_ATTRS_MAX + 1];

  cl_assert_equal_i(
      pbl_bt_ams_client_register(&s_client, PBL_BT_AMS_ENTITY_TRACK, attrs, PBL_ARRAY_SIZE(attrs)),
      -EINVAL);
  cl_assert_equal_i(s_num_writes, 0);
}

void test_ams_client__update(void) {
  static const uint8_t update[] = {0x02, 0x02, 0x01, 'S', 'o', ' ', 'W', 'h', 'a', 't'};

  fake_gattc_service_notified(CHR_ENTITY_UPDATE, update, sizeof(update));
  cl_assert_equal_i(s_num_updates, 1);
  cl_assert_equal_i(s_update.entity, PBL_BT_AMS_ENTITY_TRACK);
  cl_assert_equal_i(s_update.attr, PBL_BT_AMS_TRACK_ATTR_TITLE);
  cl_assert_equal_i(s_update.flags, PBL_BT_AMS_FLAG_TRUNCATED);
  cl_assert_equal_i(s_update.len, 7);
  cl_assert_equal_s(s_value, "So What");
}

void test_ams_client__update_empty_value(void) {
  static const uint8_t update[] = {0x00, 0x02, 0x00};

  fake_gattc_service_notified(CHR_ENTITY_UPDATE, update, sizeof(update));
  cl_assert_equal_i(s_num_updates, 1);
  cl_assert_equal_i(s_update.entity, PBL_BT_AMS_ENTITY_PLAYER);
  cl_assert_equal_i(s_update.attr, PBL_BT_AMS_PLAYER_ATTR_VOLUME);
  cl_assert_equal_i(s_update.len, 0);
}

void test_ams_client__update_too_short(void) {
  static const uint8_t update[] = {0x02, 0x02};

  fake_gattc_service_notified(CHR_ENTITY_UPDATE, update, sizeof(update));
  cl_assert_equal_i(s_num_updates, 0);
}

void test_ams_client__read_attr(void) {
  static const uint8_t cmd[] = {0x02, 0x01};

  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      0);
  cl_assert_equal_i(s_num_writes, 1);
  cl_assert_equal_i(s_written_chr, CHR_ENTITY_ATTRIBUTE);
  cl_assert_equal_i(s_written_len, sizeof(cmd));
  cl_assert_equal_m(s_written, cmd, sizeof(cmd));
  cl_assert_equal_i(fake_gattc_service_num_reads(), 0);

  /* One read at a time. */
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_TITLE),
      -EBUSY);

  fake_gattc_service_written(CHR_ENTITY_ATTRIBUTE, 0);
  cl_assert_equal_i(fake_gattc_service_num_reads(), 1);
  cl_assert_equal_i(s_num_attrs_read, 0);

  fake_gattc_service_read(CHR_ENTITY_ATTRIBUTE, 0, "Kind Of Blue", 12);
  cl_assert_equal_i(s_num_attrs_read, 1);
  cl_assert_equal_i(s_status, 0);
  cl_assert_equal_i(s_entity, PBL_BT_AMS_ENTITY_TRACK);
  cl_assert_equal_i(s_attr, PBL_BT_AMS_TRACK_ATTR_ALBUM);
  cl_assert_equal_i(s_len, 12);
  cl_assert_equal_s(s_value, "Kind Of Blue");

  /* Another can follow. */
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_TITLE),
      0);
}

void test_ams_client__read_attr_refused_by_the_service(void) {
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      0);
  fake_gattc_service_written(CHR_ENTITY_ATTRIBUTE, PBL_BT_AMS_ERR_INVALID_COMMAND);
  cl_assert_equal_i(fake_gattc_service_num_reads(), 0);
  cl_assert_equal_i(s_num_attrs_read, 1);
  cl_assert_equal_i(s_status, PBL_BT_AMS_ERR_INVALID_COMMAND);
}

void test_ams_client__read_attr_absent(void) {
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      0);
  fake_gattc_service_written(CHR_ENTITY_ATTRIBUTE, 0);
  fake_gattc_service_read(CHR_ENTITY_ATTRIBUTE, PBL_BT_AMS_ERR_ABSENT_ATTRIBUTE, nullptr, 0);
  cl_assert_equal_i(s_num_attrs_read, 1);
  cl_assert_equal_i(s_status, PBL_BT_AMS_ERR_ABSENT_ATTRIBUTE);
}

void test_ams_client__read_attr_read_refused(void) {
  fake_gattc_service_set_read_rc(-ENOMEM);
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      0);
  fake_gattc_service_written(CHR_ENTITY_ATTRIBUTE, 0);
  cl_assert_equal_i(s_num_attrs_read, 1);
  cl_assert_equal_i(s_status, -ENOMEM);
}

void test_ams_client__read_attr_write_refused(void) {
  s_write_rc = -EIO;
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      -EIO);

  s_write_rc = 0;
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      0);
}

void test_ams_client__read_attr_forgotten_when_the_service_moves(void) {
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      0);
  fake_gattc_service_written(CHR_ENTITY_ATTRIBUTE, 0);

  cl_assert_equal_i(pbl_bt_ams_client_rediscover(&s_client), 0);
  fake_gattc_service_discovered(0);
  cl_assert_equal_i(s_num_discovered, 1);

  fake_gattc_service_read(CHR_ENTITY_ATTRIBUTE, 0, "x", 1);
  cl_assert_equal_i(s_num_attrs_read, 0);

  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      0);
}

void test_ams_client__not_started(void) {
  pbl_bt_ams_client_stop(&s_client);

  cl_assert_equal_i(pbl_bt_ams_client_send_command(&s_client, PBL_BT_AMS_COMMAND_PLAY), -ENOTCONN);
  cl_assert_equal_i(
      pbl_bt_ams_client_read_attr(&s_client, PBL_BT_AMS_ENTITY_TRACK, PBL_BT_AMS_TRACK_ATTR_ALBUM),
      -ENOTCONN);
  cl_assert_equal_i(pbl_bt_ams_client_rediscover(&s_client), -ENOTCONN);
}
