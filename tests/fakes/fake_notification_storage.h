/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/timeline/item.h"

static TimelineItem s_last_stored_notification = {};
static int s_notification_store_count = 0;
static int s_notification_remove_count = 0;
static TimelineItem s_existing_ancs_notification = {
  .header = (CommonTimelineItemHeader) {
    .id = UUID_INVALID,
    .ancs_uid = 0
  }
};

extern T_STATIC bool prv_deep_copy_attributes_actions(AttributeList *attr_list,
                                                      TimelineItemActionGroup *action_group,
                                                      TimelineItem *item_out);

void fake_notification_storage_reset(void) {
  s_notification_store_count = 0;
  s_notification_remove_count = 0;
  s_existing_ancs_notification = (TimelineItem) {
    .header = (CommonTimelineItemHeader) {
      .id = UUID_INVALID,
      .ancs_uid = 0
    }
  };
}

TimelineItem *fake_notification_storage_get_last_notification(void) {
  return &s_last_stored_notification;
}

int fake_notification_storage_get_store_count(void) {
  return s_notification_store_count;
}

int fake_notification_storage_get_remove_count(void) {
  return s_notification_remove_count;
}

void fake_notification_storage_set_existing_ancs_notification(Uuid *uuid, uint32_t ancs_uid) {
  s_existing_ancs_notification = (TimelineItem) {
    .header = (CommonTimelineItemHeader) {
      .id = *uuid,
      .ancs_uid = ancs_uid
    }
  };
}

void notification_storage_init(void) {
}

void notification_storage_lock(void) {
}

void notification_storage_unlock(void) {
}

void notification_storage_store(TimelineItem *notification) {
  ++s_notification_store_count;

  // Copy notification into our last stored buffer
  timeline_item_free_allocated_buffer(&s_last_stored_notification);
  s_last_stored_notification = *notification;
  if (!prv_deep_copy_attributes_actions(&notification->attr_list, &notification->action_group,
                                        &s_last_stored_notification)) {
    s_last_stored_notification = (TimelineItem){};
  }
}

bool notification_storage_notification_exists(const Uuid *id) {
  return false;
}

size_t notification_storage_get_len(const Uuid *uuid) {
  return 0;
}

bool notification_storage_get(const Uuid *id, TimelineItem *item_out) {
  return false;
}

void notification_storage_set_status(const Uuid *id, uint8_t status) {
}

bool notification_storage_get_status(const Uuid *id, uint8_t *status) {
  return true;
}

void notification_storage_remove(const Uuid *id) {
  ++s_notification_remove_count;
}

bool notification_storage_find_ancs_notification_id(uint32_t ancs_uid, Uuid *uuid_out) {
  *uuid_out = s_existing_ancs_notification.header.id;
  return (s_existing_ancs_notification.header.ancs_uid == ancs_uid);
}

bool notification_storage_find_ancs_notification_by_timestamp(
    TimelineItem *notification, CommonTimelineItemHeader *header_out) {
  if (uuid_is_invalid(&s_existing_ancs_notification.header.id)) {
    return false;
  }
  *header_out = s_existing_ancs_notification.header;
  return true;
}

void notification_storage_rewrite(void (*iter_callback)(TimelineItem *notification,
    SerializedTimelineItemHeader *header, void *data), void *data) {
}
