/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"

#include <pbl/services/timeline/attribute.h>
#include <pbl/services/timeline/item.h>

#include <system/status_codes.h>

#include <stdbool.h>

/**
 * @defgroup services_blob_db_ios_notif_pref_db iOS notification preferences
 * @ingroup services_blob_db
 * @brief Per-app notification preferences for iOS (::BlobDBIdiOSNotifPref).
 *
 * On iOS the watch receives notifications directly from ANCS, so the phone app cannot process or
 * filter them. This database stores per-app preferences so the firmware can do it.
 * @{
 */

/** @brief Notification preferences of an iOS app. */
typedef struct {
  /** Preference attributes. */
  AttributeList attr_list;
  /** Actions offered on the app's notifications. */
  TimelineItemActionGroup action_group;
} iOSNotifPrefs;

/**
 * @brief Get the preferences of an iOS app.
 *
 * @param app_id iOS app identifier, e.g. @c com.apple.MobileSMS, not NUL-terminated.
 * @param length Length of @p app_id in bytes.
 * @return Kernel heap allocated preferences, NULL if none are stored. Free with
 * ios_notif_pref_db_free_prefs().
 */
iOSNotifPrefs *ios_notif_pref_db_get_prefs(const uint8_t *app_id, int length);

/**
 * @brief Free preferences returned by ios_notif_pref_db_get_prefs().
 *
 * @param prefs Preferences to free.
 */
void ios_notif_pref_db_free_prefs(iOSNotifPrefs *prefs);

/**
 * @brief Add or update the preferences of an iOS app.
 *
 * The record is marked dirty and synced to the phone.
 *
 * @param app_id iOS app identifier, e.g. @c com.apple.MobileSMS, not NUL-terminated.
 * @param length Length of @p app_id in bytes.
 * @param attr_list Preference attributes.
 * @param action_group Actions offered on the app's notifications.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t ios_notif_pref_db_store_prefs(const uint8_t *app_id, int length, AttributeList *attr_list,
                                       TimelineItemActionGroup *action_group);

/** @brief Initialize the iOS notification preferences database. */
void ios_notif_pref_db_init(void);

/**
 * @brief Insert or replace a record in the iOS notification preferences database.
 *
 * The key is the iOS app identifier.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t ios_notif_pref_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the iOS notification preferences database.
 *
 * The key is the iOS app identifier.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int ios_notif_pref_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the iOS notification preferences database.
 *
 * The key is the iOS app identifier.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t ios_notif_pref_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the iOS notification preferences database.
 *
 * The key is the iOS app identifier.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t ios_notif_pref_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the iOS notification preferences database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t ios_notif_pref_db_flush(void);

/**
 * @brief Compact the settings file backing the iOS notification preferences database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t ios_notif_pref_db_compact(void);

/**
 * @brief Check whether the iOS notification preferences database holds unsynced records.
 *
 * @param[out] is_dirty_out Set to true if at least one record is dirty.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t ios_notif_pref_db_is_dirty(bool *is_dirty_out);

/**
 * @brief Build the list of unsynced records of the iOS notification preferences database.
 *
 * @return Heap-allocated list (free with blob_db_util_free_dirty_list()), NULL if none.
 */
BlobDBDirtyItem *ios_notif_pref_db_get_dirty_list(void);

/**
 * @brief Mark a record of the iOS notification preferences database as synced.
 *
 * The key is the iOS app identifier.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t ios_notif_pref_db_mark_synced(const uint8_t *key, int key_len);

#if UNITTEST
/**
 * @brief Get the stored flags of an iOS app, for unit tests.
 *
 * @param app_id iOS app identifier.
 * @param key_len Length of @p app_id in bytes.
 * @return Flags.
 */
uint32_t ios_notif_pref_db_get_flags(const uint8_t *app_id, int key_len);
#endif

/** @} */
