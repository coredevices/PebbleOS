/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/api.h"
#include "services/normal/blob_db/ios_notif_pref_db.h"
#include "services/normal/timeline/item.h"

iOSNotifPrefs* ios_notif_pref_db_get_prefs(const uint8_t *app_id, int length) {
  return NULL;
}

void ios_notif_pref_db_free_prefs(iOSNotifPrefs *prefs) {
  return;
}

status_t ios_notif_pref_db_store_prefs(const uint8_t *app_id, int length, AttributeList *attr_list,
                                       TimelineItemActionGroup *action_group) {
  return S_SUCCESS;
}

void ios_notif_pref_db_init(void) {
  return;
}

status_t ios_notif_pref_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                  int val_len) {
  return S_SUCCESS;
}

int ios_notif_pref_db_get_len(const uint8_t *key, int key_len) {
  return 1;
}

status_t ios_notif_pref_db_read(const uint8_t *key, int key_len, uint8_t *val_out,
                                int val_out_len) {
  return S_SUCCESS;
}

status_t ios_notif_pref_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t ios_notif_pref_db_flush(void) {
  return S_SUCCESS;
}

status_t ios_notif_pref_db_is_dirty(bool *is_dirty_out) {
  *is_dirty_out = true;
  return S_SUCCESS;
}

BlobDBDirtyItem* ios_notif_pref_db_get_dirty_list(void) {
  return NULL;
}

status_t ios_notif_pref_db_mark_synced(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}
