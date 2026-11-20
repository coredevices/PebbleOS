/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sample.h"

#include <stdint.h>
#include <stdio.h>

#include <pbl/bluetooth/clients/ancs.h>
#include <pbl/util/size.h>

#define TITLE_MAX   32
#define MESSAGE_MAX 96

static const struct pbl_bt_ancs_attr_req s_attrs[] = {
  {.id = PBL_BT_ANCS_NOTIF_ATTR_TITLE, .max_len = TITLE_MAX},
  {.id = PBL_BT_ANCS_NOTIF_ATTR_MESSAGE, .max_len = MESSAGE_MAX},
};

/* Room for the response: its header, and each attribute with its own. */
PBL_BT_ANCS_CLIENT_DEFINE(s_client,
                          5 + 2 * sizeof(struct pbl_bt_ancs_attr) + TITLE_MAX + MESSAGE_MAX);

static void prv_discovered(struct pbl_bt_ancs_client *client, int status) {
  if (status != 0) {
    printf("ancs: not on the peer (%d)\n", status);
    return;
  }

  printf("ancs: found\n");
  /* The Data Source first, so that no response is missed. */
  (void)pbl_bt_ancs_client_subscribe(client, PBL_BT_ANCS_SOURCE_DATA, true);
  (void)pbl_bt_ancs_client_subscribe(client, PBL_BT_ANCS_SOURCE_NOTIFICATION, true);
}

static void prv_subscribed(struct pbl_bt_ancs_client *client, enum pbl_bt_ancs_source source,
                           bool enable, int status) {
  if (status != 0) {
    printf("ancs: subscribing failed (%d)\n", status);
  } else if (source == PBL_BT_ANCS_SOURCE_NOTIFICATION) {
    printf("ancs: subscribed\n");
  }
}

static void prv_notification(struct pbl_bt_ancs_client *client,
                             const struct pbl_bt_ancs_notif *notif) {
  static const char *const events[] = {"added", "modified", "removed"};

  if (notif->event >= PBL_ARRAY_SIZE(events)) {
    return;
  }

  printf("ancs: notification %u %s, category %u%s\n", (unsigned int)notif->uid,
         events[notif->event], notif->category,
         (notif->flags & PBL_BT_ANCS_FLAG_PRE_EXISTING) ? ", from before" : "");

  /* One request at a time: the attributes of a notification that comes meanwhile are not got. */
  if (notif->event != PBL_BT_ANCS_EVENT_REMOVED) {
    (void)pbl_bt_ancs_client_get_notif_attrs(client, notif->uid, s_attrs, PBL_ARRAY_SIZE(s_attrs));
  }
}

static void prv_notif_attrs(struct pbl_bt_ancs_client *client, int status, uint32_t uid,
                            const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs) {
  static const char *const names[] = {"title", "message"};

  if (status != 0) {
    /* E.g. the notification is gone since. */
    printf("ancs: request refused (%d)\n", status);
    return;
  }

  for (uint8_t i = 0; i < num_attrs; i++) {
    printf("ancs:   %s: %.*s\n", names[i], (int)attrs[i]->len, (const char *)attrs[i]->value);
  }
}

static const struct pbl_bt_ancs_client_cb s_cb = {
  .discovered = prv_discovered,
  .subscribed = prv_subscribed,
  .notification = prv_notification,
  .notif_attrs = prv_notif_attrs,
};

void sample_ancs_start(uint16_t conn_handle) {
  const int rc = pbl_bt_ancs_client_start(&s_client, conn_handle, &s_cb);

  if (rc != 0) {
    printf("ancs: starting the client failed (%d)\n", rc);
  }
}

void sample_ancs_stop(void) {
  pbl_bt_ancs_client_stop(&s_client);
}
