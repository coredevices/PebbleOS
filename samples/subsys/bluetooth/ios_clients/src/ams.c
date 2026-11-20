/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sample.h"

#include <stdint.h>
#include <stdio.h>

#include <pbl/bluetooth/clients/ams.h>
#include <pbl/util/size.h>

PBL_BT_AMS_CLIENT_DEFINE(s_client);

static const uint8_t s_player_attrs[] = {
  PBL_BT_AMS_PLAYER_ATTR_PLAYBACK_INFO,
  PBL_BT_AMS_PLAYER_ATTR_VOLUME,
};

static const uint8_t s_track_attrs[] = {
  PBL_BT_AMS_TRACK_ATTR_ARTIST,
  PBL_BT_AMS_TRACK_ATTR_ALBUM,
  PBL_BT_AMS_TRACK_ATTR_TITLE,
  PBL_BT_AMS_TRACK_ATTR_DURATION,
};

static const char *prv_attr_name(uint8_t entity, uint8_t attr) {
  static const char *const player[] = {"player", "playback", "volume"};
  static const char *const track[] = {"artist", "album", "title", "duration"};

  if (entity == PBL_BT_AMS_ENTITY_PLAYER && attr < PBL_ARRAY_SIZE(player)) {
    return player[attr];
  } else if (entity == PBL_BT_AMS_ENTITY_TRACK && attr < PBL_ARRAY_SIZE(track)) {
    return track[attr];
  }

  return "?";
}

static void prv_discovered(struct pbl_bt_ams_client *client, int status) {
  if (status != 0) {
    printf("ams: not on the peer (%d)\n", status);
    return;
  }

  printf("ams: found\n");
  (void)pbl_bt_ams_client_subscribe(client, PBL_BT_AMS_SOURCE_REMOTE_COMMAND, true);
  (void)pbl_bt_ams_client_subscribe(client, PBL_BT_AMS_SOURCE_ENTITY_UPDATE, true);
}

static void prv_subscribed(struct pbl_bt_ams_client *client, enum pbl_bt_ams_source source,
                           bool enable, int status) {
  if (status != 0) {
    printf("ams: subscribing failed (%d)\n", status);
    return;
  }

  /* The Entity Update only takes registrations once subscribed to. */
  if (source == PBL_BT_AMS_SOURCE_ENTITY_UPDATE) {
    printf("ams: subscribed\n");
    (void)pbl_bt_ams_client_register(client, PBL_BT_AMS_ENTITY_PLAYER, s_player_attrs,
                                     PBL_ARRAY_SIZE(s_player_attrs));
    (void)pbl_bt_ams_client_register(client, PBL_BT_AMS_ENTITY_TRACK, s_track_attrs,
                                     PBL_ARRAY_SIZE(s_track_attrs));
  }
}

static void prv_commands(struct pbl_bt_ams_client *client, const uint8_t *commands,
                         uint8_t num_commands) {
  printf("ams: the player takes commands");
  for (uint8_t i = 0; i < num_commands; i++) {
    printf(" %u", commands[i]);
  }
  printf("\n");
}

static void prv_update(struct pbl_bt_ams_client *client, const struct pbl_bt_ams_update *update) {
  /* One read at a time: a value cut while another is being read stays cut. */
  if ((update->flags & PBL_BT_AMS_FLAG_TRUNCATED) &&
      pbl_bt_ams_client_read_attr(client, update->entity, update->attr) == 0) {
    return;
  }

  printf("ams: %s: %.*s\n", prv_attr_name(update->entity, update->attr), (int)update->len,
         (const char *)update->value);
}

static void prv_registered(struct pbl_bt_ams_client *client, int status) {
  if (status != 0) {
    printf("ams: registering failed (%d)\n", status);
  }
}

static void prv_attr_read(struct pbl_bt_ams_client *client, int status,
                          enum pbl_bt_ams_entity entity, uint8_t attr, const uint8_t *value,
                          uint16_t len) {
  if (status != 0) {
    printf("ams: reading failed (%d)\n", status);
    return;
  }

  printf("ams: %s: %.*s\n", prv_attr_name(entity, attr), (int)len, (const char *)value);
}

static const struct pbl_bt_ams_client_cb s_cb = {
  .discovered = prv_discovered,
  .subscribed = prv_subscribed,
  .commands = prv_commands,
  .update = prv_update,
  .registered = prv_registered,
  .attr_read = prv_attr_read,
};

void sample_ams_start(uint16_t conn_handle) {
  const int rc = pbl_bt_ams_client_start(&s_client, conn_handle, &s_cb);

  if (rc != 0) {
    printf("ams: starting the client failed (%d)\n", rc);
  }
}

void sample_ams_stop(void) {
  pbl_bt_ams_client_stop(&s_client);
}
