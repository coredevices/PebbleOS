/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"

#include "services/normal/timeline/attribute.h"
#include "services/normal/timeline/item.h"

#include "system/status_codes.h"

#include <stdbool.h>

//! The iOS Pebble app doesn't have much control over the notification experience.
//! The watch receives notifications directly from ANCS, so the iOS app doesn't get a
//! chance to do any processing or filtering.
//! This db stores preferences on different types of notifications so the FW can perform
//! some processing / filtering.


typedef struct {
  AttributeList attr_list;
  TimelineItemActionGroup action_group;
} iOSNotifPrefs;

//! @param app_id The iOS app id to check. ex com.apple.MobileSMS
//! @param length The length of the app_id
//! @return A pointer to the prefs, NULL if none are available
//! @note The caller must cleanup with ios_notif_pref_db_free_prefs()
iOSNotifPrefs* ios_notif_pref_db_get_prefs(const uint8_t *app_id, int length);

//! @param prefs A pointer to prefs returned by ios_notif_pref_db_get_prefs()
void ios_notif_pref_db_free_prefs(iOSNotifPrefs *prefs);

//! Adds or updates a record in the notif_pref_db.
//! @param app_id The iOS app id to check. ex com.apple.MobileSMS
//! @param length The length of the app_id
//! @param attr_list AttributeList for the app
//! @param attr_list ActionGroup for the app
status_t ios_notif_pref_db_store_prefs(const uint8_t *app_id, int length, AttributeList *attr_list,
                                       TimelineItemActionGroup *action_group);

///////////////////////////////////////////
// BlobDB Boilerplate (see blob_db/api.h)
///////////////////////////////////////////

void ios_notif_pref_db_init(void);

status_t ios_notif_pref_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

int ios_notif_pref_db_get_len(const uint8_t *key, int key_len);

status_t ios_notif_pref_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

status_t ios_notif_pref_db_delete(const uint8_t *key, int key_len);

status_t ios_notif_pref_db_flush(void);

status_t ios_notif_pref_db_is_dirty(bool *is_dirty_out);

BlobDBDirtyItem* ios_notif_pref_db_get_dirty_list(void);

status_t ios_notif_pref_db_mark_synced(const uint8_t *key, int key_len);

#if UNITTEST
uint32_t ios_notif_pref_db_get_flags(const uint8_t *app_id, int key_len);
#endif
