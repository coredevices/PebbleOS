/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/weather/weather_service_private.h>

#include <apps/system/reminders/reminder_prefs.h>
#include <apps/system/send_text/prefs.h>
#include <system/status_codes.h>

/**
 * @defgroup services_blob_db_watch_app_prefs_db Watch app preferences
 * @ingroup services_blob_db
 * @brief Preferences of system apps set from the phone (::BlobDBIdWatchAppPrefs).
 *
 * Records are keyed by a per-app string.
 * @{
 */

/**
 * @brief Read the Send Text app preferences.
 *
 * @return Task heap allocated preferences, NULL on failure. Free with task_free().
 */
SerializedSendTextPrefs *watch_app_prefs_get_send_text(void);

/**
 * @brief Read the Weather app location ordering.
 *
 * @return Task heap allocated preferences, NULL on failure. Free with
 * watch_app_prefs_destroy_weather().
 */
SerializedWeatherAppPrefs *watch_app_prefs_get_weather(void);

/**
 * @brief Read the Reminders app preferences.
 *
 * Served from a cache after the first read.
 *
 * @return Task heap allocated preferences, NULL on failure. Free with task_free().
 */
SerializedReminderAppPrefs *watch_app_prefs_get_reminder(void);

/**
 * @brief Free preferences returned by watch_app_prefs_get_weather().
 *
 * @param prefs Preferences, may be NULL.
 */
void watch_app_prefs_destroy_weather(SerializedWeatherAppPrefs *prefs);

/** @brief Initialize the watch app preferences database. */
void watch_app_prefs_db_init(void);

/**
 * @brief Insert or replace a record in the watch app preferences database.
 *
 * Only the Send Text, Weather and Reminders app keys are accepted; list sizes are validated.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t watch_app_prefs_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                   int val_len);

/**
 * @brief Get the length of a record in the watch app preferences database.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int watch_app_prefs_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the watch app preferences database.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t watch_app_prefs_db_read(const uint8_t *key, int key_len, uint8_t *val_out,
                                 int val_out_len);

/**
 * @brief Delete a record from the watch app preferences database.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t watch_app_prefs_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the watch app preferences database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t watch_app_prefs_db_flush(void);

/**
 * @brief Compact the settings file backing the watch app preferences database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t watch_app_prefs_db_compact(void);

/** @} */
