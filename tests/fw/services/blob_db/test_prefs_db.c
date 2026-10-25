/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/blob_db/prefs_db.h>
#include <pbl/services/filesystem/pfs.h>
#include <pbl/services/touch/touch.h>
#include <pbl/services/touch/touch_nav_service.h>
#include <pbl/util/size.h>
#include <pbl/util/uuid.h>

#include <clar.h>
#include <process_management/app_install_manager.h>
#include <shell/normal/quick_launch.h>
#include <shell/prefs.h>
#include <shell/prefs_private.h>

// Fixture
////////////////////////////////////////////////////////////////

typedef struct {
  bool enabled;
  Uuid uuid;
} SwipePreference;

static const struct {
  SwipeDirection direction;
  const char *key;
} s_swipes[] = {
  {SwipeDirection_Up, "qlSwipeUp"},
  {SwipeDirection_Down, "qlSwipeDown"},
  {SwipeDirection_Left, "qlSwipeLeft"},
  {SwipeDirection_Right, "qlSwipeRight"},
};

static const Uuid s_app_uuid = {0, 1, 2, 3};
static const AppInstallId s_app_id = 42;
static Uuid s_last_lookup_uuid;
static unsigned s_tick_events;
#ifdef CONFIG_TOUCH
static bool s_touch_service_enabled;
static bool s_touch_nav_enabled;
static unsigned s_nav_changes;
static unsigned s_master_changes;

void touch_service_set_globally_enabled(bool enabled) {
  s_touch_service_enabled = enabled;
}

void touch_nav_set_enabled(bool enabled) {
  s_touch_nav_enabled = enabled;
  s_nav_changes++;
}

void touch_set_backlight_enabled(bool enabled) {
}

void touch_nav_master_changed(void) {
  s_master_changes++;
}
#endif

// Fakes
////////////////////////////////////////////////////////////////
#include <fake_kernel_services_notifications.h>
#include <fake_spi_flash.h>
#include <fake_system_task.h>

// Stubs
////////////////////////////////////////////////////////////////

AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  s_last_lookup_uuid = *uuid;
  return uuid_equal(uuid, &s_app_uuid) ? s_app_id : INSTALL_ID_INVALID;
}

bool app_install_get_uuid_for_install_id(AppInstallId app_id, Uuid *uuid) {
  *uuid = app_id == s_app_id ? s_app_uuid : UUID_INVALID;
  return app_id == s_app_id;
}

void prefs_sync_init(void) {
}

void event_put(PebbleEvent *event) {
  if (event->type == PEBBLE_TICK_EVENT) {
    s_tick_events++;
  }
}

void i18n_enable(bool enable) {
}

void test_prefs_db__initialize(void) {
  // Check static defaults before any test cleanup can reset them.
  static bool checked_defaults;
  if (!checked_defaults) {
    for (size_t i = 0; i < ARRAY_LENGTH(s_swipes); ++i) {
      cl_assert(!quick_launch_swipe_is_enabled(s_swipes[i].direction));
      cl_assert_equal_i(quick_launch_swipe_get_app(s_swipes[i].direction), INSTALL_ID_INVALID);
      cl_assert(uuid_is_invalid(&s_last_lookup_uuid));
    }
    checked_defaults = true;
  }
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  touch_set_globally_enabled(true);
  touch_set_navigation_menu_enabled(true);
#ifdef CONFIG_TOUCH
  s_touch_nav_enabled = true;
  s_nav_changes = 0;
  s_master_changes = 0;
#endif
  s_tick_events = 0;
}

void test_prefs_db__cleanup(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_swipes); ++i) {
    quick_launch_swipe_set_app(s_swipes[i].direction, INSTALL_ID_INVALID);
  }
}

void test_prefs_db__get_length(void) {
  Uuid uuid = {0, 1, 2, 3};
  const char *key = "workerId";
  int key_len = strlen(key);
  cl_assert_equal_i(prefs_db_insert((uint8_t *)key, key_len, (uint8_t *)&uuid, sizeof(uuid)), 0);
  cl_assert_equal_i(prefs_db_get_len((uint8_t *)key, key_len), sizeof(uuid));
}

void test_prefs_db__insert_and_read(void) {
  uint32_t set_value = 42;

  // NOTE: We intentionally put one garbage character after the key to catch errors
  // that assume the key is 0 terminated
  const char *key = "lightTimeoutMsX";
  int key_len = strlen(key) - 1;

  // Set initial value
  backlight_set_timeout_ms(set_value + 1);

  // Insert and check the length
  cl_assert_equal_i(
      prefs_db_insert((uint8_t *)key, key_len, (uint8_t *)&set_value, sizeof(set_value)), 0);
  cl_assert_equal_i(prefs_db_get_len((uint8_t *)key, key_len), sizeof(set_value));

  // Read it back
  uint32_t get_value;
  cl_assert_equal_i(
      prefs_db_read((uint8_t *)key, key_len, (uint8_t *)&get_value, sizeof(get_value)), 0);
  cl_assert_equal_i(set_value, get_value);

  // If we get the pref setting now, it should still be the old value because we haven't
  // issued the blob_db update event yet
  uint32_t get_pref = backlight_get_timeout_ms();
  cl_assert_equal_i(get_pref, set_value + 1);

  // Issue the blob_db update event
  PebbleBlobDBEvent event = (PebbleBlobDBEvent){
    .db_id = BlobDBIdPrefs,
    .type = BlobDBEventTypeInsert,
    .key = (uint8_t *)key,
    .key_len = key_len,
  };
  prefs_private_handle_blob_db_event(&event);
  get_pref = backlight_get_timeout_ms();
  cl_assert(get_pref == get_value);

  // Set new value using the set call and read it back using prefs_db
  uint32_t new_set_value = 4242;
  backlight_set_timeout_ms(new_set_value);
  cl_assert_equal_i(
      prefs_db_read((uint8_t *)key, key_len, (uint8_t *)&get_value, sizeof(get_value)), 0);
  cl_assert_equal_i(new_set_value, get_value);

  // Try and insert an unknown key. It should fail
  const char *bad_key = "bad_key";
  int bad_key_len = strlen(bad_key);
  cl_assert(prefs_db_insert((uint8_t *)bad_key, bad_key_len, (uint8_t *)&set_value,
                            sizeof(set_value)) < 0);
  cl_assert(prefs_db_get_len((uint8_t *)bad_key, bad_key_len) < 0);
  cl_assert(
      prefs_db_read((uint8_t *)bad_key, bad_key_len, (uint8_t *)&get_value, sizeof(get_value)) < 0);

  // Try and insert the wrong size for a known key, it should fail
  cl_assert(prefs_db_insert((uint8_t *)key, key_len, (uint8_t *)&set_value, sizeof(set_value) + 1) <
            0);
  // Read it back
  cl_assert(prefs_db_read((uint8_t *)key, key_len, (uint8_t *)&get_value, sizeof(get_value) + 1) <
            0);
}

static SwipePreference prv_read_swipe_pref(const char *key) {
  SwipePreference pref;
  cl_assert_equal_i(prefs_db_get_len((const uint8_t *)key, strlen(key)), sizeof(pref));
  cl_assert_equal_i(
      prefs_db_read((const uint8_t *)key, strlen(key), (uint8_t *)&pref, sizeof(pref)), S_SUCCESS);
  return pref;
}

static void prv_notify_pref(const char *key) {
  PebbleBlobDBEvent event = {
    .db_id = BlobDBIdPrefs,
    .type = BlobDBEventTypeInsert,
    .key = (uint8_t *)key,
    .key_len = strlen(key),
  };
  prefs_private_handle_blob_db_event(&event);
}

void test_prefs_db__swipe_defaults(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_swipes); ++i) {
    cl_assert(!quick_launch_swipe_is_enabled(s_swipes[i].direction));
    cl_assert_equal_i(quick_launch_swipe_get_app(s_swipes[i].direction), INSTALL_ID_INVALID);
    quick_launch_swipe_set_enabled(s_swipes[i].direction, false);
    SwipePreference pref = prv_read_swipe_pref(s_swipes[i].key);
    cl_assert(!pref.enabled);
    cl_assert(uuid_is_invalid(&pref.uuid));
  }
}

void test_prefs_db__swipe_round_trip(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_swipes); ++i) {
    const SwipeDirection direction = s_swipes[i].direction;
    const char *key = s_swipes[i].key;
    quick_launch_swipe_set_app(direction, s_app_id);
    cl_assert(quick_launch_swipe_is_enabled(direction));
    cl_assert_equal_i(quick_launch_swipe_get_app(direction), s_app_id);
    SwipePreference pref = prv_read_swipe_pref(key);
    cl_assert(pref.enabled);
    cl_assert(uuid_equal(&pref.uuid, &s_app_uuid));

    quick_launch_swipe_set_enabled(direction, false);
    cl_assert(!quick_launch_swipe_is_enabled(direction));
    cl_assert_equal_i(quick_launch_swipe_get_app(direction), s_app_id);
    pref = prv_read_swipe_pref(key);
    cl_assert(!pref.enabled);
    cl_assert(uuid_equal(&pref.uuid, &s_app_uuid));

    pref.enabled = true;
    cl_assert_equal_i(
        prefs_db_insert((const uint8_t *)key, strlen(key), (uint8_t *)&pref, sizeof(pref)),
        S_SUCCESS);
    cl_assert(!quick_launch_swipe_is_enabled(direction));
    prv_notify_pref(key);
    cl_assert(quick_launch_swipe_is_enabled(direction));
    cl_assert_equal_i(quick_launch_swipe_get_app(direction), s_app_id);
    quick_launch_swipe_set_app(direction, INSTALL_ID_INVALID);
  }
}

void test_prefs_db__swipe_normalizes_invalid_uuid(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_swipes); ++i) {
    const SwipeDirection direction = s_swipes[i].direction;
    const char *key = s_swipes[i].key;
    quick_launch_swipe_set_app(direction, s_app_id);
    SwipePreference pref = {.enabled = true, .uuid = UUID_INVALID_INIT};
    cl_assert_equal_i(
        prefs_db_insert((const uint8_t *)key, strlen(key), (uint8_t *)&pref, sizeof(pref)),
        S_SUCCESS);
    prv_notify_pref(key);
    cl_assert(!quick_launch_swipe_is_enabled(direction));
    cl_assert_equal_i(quick_launch_swipe_get_app(direction), INSTALL_ID_INVALID);

    quick_launch_swipe_set_enabled(direction, true);
    pref = prv_read_swipe_pref(key);
    cl_assert(!pref.enabled);
    cl_assert(uuid_is_invalid(&pref.uuid));
    quick_launch_swipe_set_app(direction, INSTALL_ID_INVALID);
    cl_assert(!quick_launch_swipe_is_enabled(direction));
  }
}

void test_prefs_db__swipe_rejects_wrong_sizes(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_swipes); ++i) {
    const char *key = s_swipes[i].key;
    quick_launch_swipe_set_app(s_swipes[i].direction, s_app_id);
    uint8_t buffer[sizeof(SwipePreference) + 1] = {};
    const size_t sizes[] = {0, sizeof(SwipePreference) - 1, sizeof(SwipePreference) + 1};
    for (size_t j = 0; j < ARRAY_LENGTH(sizes); ++j) {
      cl_assert_equal_i(prefs_db_insert((const uint8_t *)key, strlen(key), buffer, sizes[j]),
                        E_INVALID_ARGUMENT);
      cl_assert_equal_i(prefs_db_read((const uint8_t *)key, strlen(key), buffer, sizes[j]),
                        E_INVALID_ARGUMENT);
    }
    prv_notify_pref(key);
    cl_assert(quick_launch_swipe_is_enabled(s_swipes[i].direction));
    cl_assert_equal_i(quick_launch_swipe_get_app(s_swipes[i].direction), s_app_id);
    SwipePreference pref = prv_read_swipe_pref(key);
    cl_assert(pref.enabled);
    cl_assert(uuid_equal(&pref.uuid, &s_app_uuid));
  }
}

static void prv_insert_bool_pref(const char *key, bool value) {
  cl_assert_equal_i(
      prefs_db_insert((const uint8_t *)key, strlen(key), (uint8_t *)&value, sizeof(value)),
      S_SUCCESS);
  prv_notify_pref(key);
}

void test_prefs_db__clock_change_notifies_watchface(void) {
  shell_prefs_set_clock_24h_style(false);
  s_tick_events = 0;
  prv_insert_bool_pref("clock24h", true);
  cl_assert(shell_prefs_get_clock_24h_style());
  cl_assert_equal_i(s_tick_events, 1);
  prv_insert_bool_pref("clock24h", true);
  cl_assert_equal_i(s_tick_events, 1);
  prv_insert_bool_pref("clock24h", false);
  cl_assert(!shell_prefs_get_clock_24h_style());
  cl_assert_equal_i(s_tick_events, 2);
}

void test_prefs_db__touch_navigation_changes_runtime_gate(void) {
  prv_insert_bool_pref("touchNavMenuEnabled", false);
  cl_assert(!touch_navigation_menu_is_enabled());
  cl_assert(touch_is_globally_enabled());
#ifdef CONFIG_TOUCH
  cl_assert(s_touch_service_enabled);
  cl_assert(!s_touch_nav_enabled);
  cl_assert_equal_i(s_nav_changes, 1);
#endif
  prv_insert_bool_pref("touchNavMenuEnabled", false);
#ifdef CONFIG_TOUCH
  cl_assert_equal_i(s_nav_changes, 1);
#endif
  prv_insert_bool_pref("touchNavMenuEnabled", true);
  cl_assert(touch_navigation_menu_is_enabled());
#ifdef CONFIG_TOUCH
  cl_assert(s_touch_nav_enabled);
  cl_assert_equal_i(s_nav_changes, 2);
  cl_assert_equal_i(s_master_changes, 0);
#endif
}

void test_prefs_db__touch_master_changes_runtime_gate(void) {
  prv_insert_bool_pref("touchEnabled", false);
  cl_assert(!touch_is_globally_enabled());
  cl_assert(touch_navigation_menu_is_enabled());
#ifdef CONFIG_TOUCH
  cl_assert(!s_touch_service_enabled);
  cl_assert(!s_touch_nav_enabled);
  cl_assert_equal_i(s_nav_changes, 1);
#endif
  prv_insert_bool_pref("touchEnabled", true);
  cl_assert(touch_is_globally_enabled());
#ifdef CONFIG_TOUCH
  cl_assert(s_touch_service_enabled);
  cl_assert(s_touch_nav_enabled);
  cl_assert_equal_i(s_nav_changes, 2);
  cl_assert_equal_i(s_master_changes, 0);
#endif
}

void test_prefs_db__touch_master_resyncs_opted_in_apps_with_navigation_off(void) {
  prv_insert_bool_pref("touchNavMenuEnabled", false);
  prv_insert_bool_pref("touchEnabled", false);
  cl_assert(!touch_is_globally_enabled());
  cl_assert(!touch_navigation_menu_is_enabled());
#ifdef CONFIG_TOUCH
  cl_assert(!s_touch_service_enabled);
  cl_assert(!s_touch_nav_enabled);
  cl_assert_equal_i(s_nav_changes, 1);
  cl_assert_equal_i(s_master_changes, 1);
#endif
  prv_insert_bool_pref("touchEnabled", false);
#ifdef CONFIG_TOUCH
  cl_assert_equal_i(s_master_changes, 1);
#endif
  prv_insert_bool_pref("touchEnabled", true);
  cl_assert(touch_is_globally_enabled());
  cl_assert(!touch_navigation_menu_is_enabled());
#ifdef CONFIG_TOUCH
  cl_assert(s_touch_service_enabled);
  cl_assert(!s_touch_nav_enabled);
  cl_assert_equal_i(s_nav_changes, 1);
  cl_assert_equal_i(s_master_changes, 2);
#endif
}
