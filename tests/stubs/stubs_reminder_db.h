/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/timeline_item_storage.h"
#include "services/normal/settings/settings_file.h"

void reminder_db_init(void) {
  return;
}

status_t reminder_db_insert_item(TimelineItem *item) {
  return S_SUCCESS;
}

status_t reminder_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  return S_SUCCESS;
}

int reminder_db_get_len(const uint8_t *key, int key_len) {
  return 1;
}

status_t reminder_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  return S_SUCCESS;
}

bool reminder_db_find_by_timestamp_title(time_t timestamp, const char *title,
                                         TimelineItemStorageFilterCallback filter_cb,
                                         TimelineItem *reminder_out) {
  return false;
}

status_t reminder_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t reminder_db_flush(void) {
  return S_SUCCESS;
}

status_t reminder_db_each(SettingsFileEachCallback each, void *data) {
  return S_SUCCESS;
}

status_t reminder_db_read_item(TimelineItem *item_out, TimelineItemId *id) {
  return S_SUCCESS;
}

status_t reminder_db_delete_item(const TimelineItemId *id, bool send_event) {
  return S_SUCCESS;
}

bool reminder_db_is_empty(void) {
  return false;
}

status_t reminder_db_delete_with_parent(const TimelineItemId *parent_id) {
  return S_SUCCESS;
}

status_t reminder_db_is_dirty(bool *is_dirty_out) {
  return S_SUCCESS;
}

BlobDBDirtyItem* reminder_db_get_dirty_list(void) {
  return NULL;
}

status_t reminder_db_mark_synced(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}
