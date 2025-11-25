/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "send_text_service.h"

#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "services/common/bluetooth/bluetooth_persistent_storage.h"
#include "services/normal/blob_db/ios_notif_pref_db.h"
#include "services/normal/notifications/notification_constants.h"


static bool s_has_send_text_reply_action = false;

static bool prv_has_send_text_reply_action(void) {
  iOSNotifPrefs *notif_prefs = ios_notif_pref_db_get_prefs((uint8_t *)SEND_TEXT_NOTIF_PREF_KEY,
                                                           strlen(SEND_TEXT_NOTIF_PREF_KEY));
  if (!notif_prefs) {
    return false;
  }

  bool has_reply_action =
      (timeline_item_action_group_find_reply_action(&notif_prefs->action_group) != NULL);

  ios_notif_pref_db_free_prefs(notif_prefs);
  return has_reply_action;
}

static void prv_blobdb_event_handler(PebbleEvent *event, void *context) {
  const PebbleBlobDBEvent *blobdb_event = &event->blob_db;
  if (blobdb_event->db_id != BlobDBIdiOSNotifPref) {
    return;
  }

  if (blobdb_event->type != BlobDBEventTypeFlush &&
      memcmp(blobdb_event->key, (uint8_t *)SEND_TEXT_NOTIF_PREF_KEY,
             strlen(SEND_TEXT_NOTIF_PREF_KEY))) {
    return;
  }

  s_has_send_text_reply_action = prv_has_send_text_reply_action();
}

void send_text_service_init(void) {
  // Save the initial state
  s_has_send_text_reply_action = prv_has_send_text_reply_action();;

  // Register for updates
  static EventServiceInfo s_blobdb_event_info = {
    .type = PEBBLE_BLOBDB_EVENT,
    .handler = prv_blobdb_event_handler,
  };
  event_service_client_subscribe(&s_blobdb_event_info);
}

bool send_text_service_is_send_text_supported(void) {
  PebbleProtocolCapabilities capabilities;
  bt_persistent_storage_get_cached_system_capabilities(&capabilities);

  return (capabilities.send_text_support && s_has_send_text_reply_action);
}
