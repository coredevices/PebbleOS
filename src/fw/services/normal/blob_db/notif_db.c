/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "notif_db.h"

#include "kernel/pbl_malloc.h"
#include "services/normal/notifications/notification_storage.h"
#include "system/logging.h"

void notif_db_init(void) {
}

status_t notif_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  if (key_len != UUID_SIZE ||
      val_len < (int)sizeof(SerializedTimelineItemHeader)) {
    return E_INVALID_ARGUMENT;
  }

  // [FBO] this is a little bit silly: we deserialize the item to then re-serialize it in
  // notification_storage_store. It has the advantage that it validates the payload
  // and works with the existing storage
  SerializedTimelineItemHeader *hdr = (SerializedTimelineItemHeader *)val;
  const uint8_t *payload = val + sizeof(SerializedTimelineItemHeader);

  const bool has_status_bits = (hdr->common.status != 0);

  TimelineItem notification = {};
  if (!timeline_item_deserialize_item(&notification, hdr, payload)) {
    return E_INTERNAL;
  }

  Uuid *id = kernel_malloc_check(sizeof(Uuid));
  *id = notification.header.id;

  char uuid_string[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(id, uuid_string);

  // If the notification already exists, only update the status flags
  if (notification_storage_notification_exists(&notification.header.id)) {
    notification_storage_set_status(&notification.header.id, notification.header.status);
    PBL_LOG(LOG_LEVEL_INFO, "Notification modified: %s", uuid_string);
    notifications_handle_notification_acted_upon(id);
  } else if (!has_status_bits) {
    notification_storage_store(&notification);
    PBL_LOG(LOG_LEVEL_INFO, "Notification added: %s", uuid_string);
    notifications_handle_notification_added(id);
  }

  timeline_item_free_allocated_buffer(&notification);
  return S_SUCCESS;
}

int notif_db_get_len(const uint8_t *key, int key_len) {
  if (key_len < UUID_SIZE) {
    return 0;
  }

  return notification_storage_get_len((Uuid *)key);
}

status_t notif_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len) {
  // NYI
  return S_SUCCESS;
}

status_t notif_db_delete(const uint8_t *key, int key_len) {
  if (key_len != UUID_SIZE) {
    return E_INVALID_ARGUMENT;
  }

  notification_storage_remove((Uuid *)key);
  notifications_handle_notification_removed((Uuid *)key);

  return S_SUCCESS;
}

status_t notif_db_flush(void) {
  notification_storage_reset_and_init();
  return S_SUCCESS;
}
