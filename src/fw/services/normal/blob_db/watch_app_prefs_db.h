/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "apps/system_apps/send_text/send_text_app_prefs.h"
#include "apps/system_apps/reminders/reminder_app_prefs.h"
#include "services/normal/weather/weather_service_private.h"
#include "system/status_codes.h"

// Reads the Send Text app prefs from the db
// @return Pointer to new \ref SerializedSendTextPrefs, NULL on failure
// @note task_free() must be called on the pointer when done with the memory
SerializedSendTextPrefs *watch_app_prefs_get_send_text(void);

// Reads the Weather app location ordering from the db
// @return pointer to new \ref SerializedWeatherAppPrefs, NULL on failure
// @note use weather_app_prefs_destroy_weather() to free memory allocated by this method
SerializedWeatherAppPrefs *watch_app_prefs_get_weather(void);

// Reads the Reminder App prefs from the db
// @return pointer to new \ref SerializedRemindersAppPrefs, NULL on failure
// @note task_free() must be called on the pointer when done with the memory
SerializedReminderAppPrefs *watch_app_prefs_get_reminder(void);

// Frees memory allocated from watch_app_prefs_get_weather()
void watch_app_prefs_destroy_weather(SerializedWeatherAppPrefs *prefs);

///////////////////////////////////////////
// BlobDB Boilerplate (see blob_db/api.h)
///////////////////////////////////////////

void watch_app_prefs_db_init(void);

status_t watch_app_prefs_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                   int val_len);

int watch_app_prefs_db_get_len(const uint8_t *key, int key_len);

status_t watch_app_prefs_db_read(const uint8_t *key, int key_len, uint8_t *val_out,
                                 int val_out_len);

status_t watch_app_prefs_db_delete(const uint8_t *key, int key_len);

status_t watch_app_prefs_db_flush(void);
