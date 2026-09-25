/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/notifications/alerts_preferences_private.h"

#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/settings/settings_file.h"
#include "shell/prefs_private.h"

#include <string.h>

#include "clar.h"

#include "stubs_analytics.h"
#include "stubs_do_not_disturb.h"
#include "stubs_events.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_rtc.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"
#include "stubs_vibe_score_info.h"

#include "fake_spi_flash.h"

#define NOTIF_PREFS_FILE_NAME         "notifpref"
#define NOTIF_PREFS_FILE_LEN          1024
#define PREF_KEY_NOTIF_TEXT_SIZE      "notifTextSize"
#define PREF_KEY_TEXT_STYLE           "textStyle"
#define PREF_KEY_NOTIF_WINDOW_TIMEOUT "notifWindowTimeout"

static void prv_set(const char *file_name, int file_len, const char *key, size_t key_len,
                    const void *val, size_t val_len) {
  SettingsFile file = {{0}};
  cl_assert_equal_i(settings_file_open(&file, file_name, file_len), S_SUCCESS);
  cl_assert_equal_i(settings_file_set(&file, key, key_len, val, val_len), S_SUCCESS);
  settings_file_close(&file);
}

// Shell pref keys are stored with their NUL terminator, notification pref keys without.
static void prv_set_system_text_style(uint8_t style) {
  prv_set(SHELL_PREFS_FILE_NAME, SHELL_PREFS_FILE_LEN, PREF_KEY_TEXT_STYLE,
          sizeof(PREF_KEY_TEXT_STYLE), &style, sizeof(style));
}

static void prv_set_notification_text_size(PreferredContentSize size) {
  prv_set(NOTIF_PREFS_FILE_NAME, NOTIF_PREFS_FILE_LEN, PREF_KEY_NOTIF_TEXT_SIZE,
          strlen(PREF_KEY_NOTIF_TEXT_SIZE), &size, sizeof(size));
}

static bool prv_notification_text_size_stored(void) {
  SettingsFile file = {{0}};
  cl_assert_equal_i(settings_file_open(&file, NOTIF_PREFS_FILE_NAME, NOTIF_PREFS_FILE_LEN),
                    S_SUCCESS);
  const bool exists =
      settings_file_exists(&file, PREF_KEY_NOTIF_TEXT_SIZE, strlen(PREF_KEY_NOTIF_TEXT_SIZE));
  settings_file_close(&file);
  return exists;
}

void test_alerts_preferences__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
}

void test_alerts_preferences__cleanup(void) {
}

void test_alerts_preferences__text_size_defaults_to_platform(void) {
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_content_size(),
                    PreferredContentSizeDefault);
  cl_assert(prv_notification_text_size_stored());
}

void test_alerts_preferences__text_size_migrates_from_system(void) {
  prv_set_system_text_style(PreferredContentSizeSmall);
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_content_size(), PreferredContentSizeSmall);
  cl_assert(prv_notification_text_size_stored());

  // The migrated value sticks even when the system Text Size changes afterwards.
  prv_set_system_text_style(PreferredContentSizeLarge);
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_content_size(), PreferredContentSizeSmall);
}

void test_alerts_preferences__text_size_keeps_existing_pref(void) {
  prv_set_system_text_style(PreferredContentSizeSmall);
  prv_set_notification_text_size(NotificationContentSizeSystem);
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_content_size(),
                    NotificationContentSizeSystem);
}

void test_alerts_preferences__text_size_ignores_invalid_system_value(void) {
  prv_set_system_text_style(0xff);
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_content_size(),
                    PreferredContentSizeDefault);
}

static void prv_set_notification_window_timeout(uint32_t timeout_ms) {
  prv_set(NOTIF_PREFS_FILE_NAME, NOTIF_PREFS_FILE_LEN, PREF_KEY_NOTIF_WINDOW_TIMEOUT,
          strlen(PREF_KEY_NOTIF_WINDOW_TIMEOUT), &timeout_ms, sizeof(timeout_ms));
}

void test_alerts_preferences__window_timeout_below_minimum(void) {
  prv_set_notification_window_timeout(0);
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_window_timeout_ms(),
                    NOTIF_WINDOW_TIMEOUT_MIN);
}

void test_alerts_preferences__window_timeout_keeps_valid_values(void) {
  prv_set_notification_window_timeout(30 * MS_PER_SECOND);
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_window_timeout_ms(), 30 * MS_PER_SECOND);

  prv_set_notification_window_timeout(NOTIF_WINDOW_TIMEOUT_INFINITE);
  alerts_preferences_init();
  cl_assert(alerts_preferences_get_notification_window_timeout_ms() ==
            NOTIF_WINDOW_TIMEOUT_INFINITE);
}

void test_alerts_preferences__hold_select_action_round_trip(void) {
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_hold_select_action(),
                    NotificationHoldSelectAction_DismissAll);

  alerts_preferences_set_notification_hold_select_action(NotificationHoldSelectAction_Clear);
  alerts_preferences_init();
  cl_assert_equal_i(alerts_preferences_get_notification_hold_select_action(),
                    NotificationHoldSelectAction_Clear);
}
