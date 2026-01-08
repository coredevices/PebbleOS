/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/menu_preferences.h"
#include "services/normal/settings/settings_file.h"
#include "services/normal/vibes/vibe_intensity.h"
#include "os/mutex.h"

#include <string.h>

#define FILE_NAME "menupref"
#define FILE_LEN (1024)

static PebbleMutex *s_mutex;

////////////////////
//! Preference keys
////////////////////

#define PREF_KEY_SCROLL_WRAP_AROUND "scrollWrapAround"
static bool s_scroll_wrap_around = false;

#define PREF_KEY_SCROLL_VIBE_ON_WRAP "scrollVibeOnWrap"
static bool s_scroll_vibe_on_wrap = false;

#define PREF_KEY_SCROLL_VIBE_ON_BLOCKED "scrollVibeOnBlocked"
static bool s_scroll_vibe_on_blocked = false;

void menu_preferences_init(void) {
  s_mutex = mutex_create();

  SettingsFile file = {{0}};
  if (settings_file_open(&file, FILE_NAME, FILE_LEN) != S_SUCCESS) {
      return;
  }

  (void)settings_file_get(&file, PREF_KEY_SCROLL_WRAP_AROUND, strlen(PREF_KEY_SCROLL_WRAP_AROUND), &s_scroll_wrap_around, sizeof(s_scroll_wrap_around));
  (void)settings_file_get(&file, PREF_KEY_SCROLL_VIBE_ON_WRAP, strlen(PREF_KEY_SCROLL_VIBE_ON_WRAP), &s_scroll_vibe_on_wrap, sizeof(s_scroll_vibe_on_wrap));
  (void)settings_file_get(&file, PREF_KEY_SCROLL_VIBE_ON_BLOCKED, strlen(PREF_KEY_SCROLL_VIBE_ON_BLOCKED), &s_scroll_vibe_on_blocked, sizeof(s_scroll_vibe_on_blocked));

  settings_file_close(&file);
}

// Convenience macro for setting a string key to a non-pointer value.
#define SET_PREF(key, value) \
  prv_set_pref(key, strlen(key), &value, sizeof(value))

static void prv_set_pref(const void *key, size_t key_len, const void *value,
                         size_t value_len) {
  mutex_lock(s_mutex);
  SettingsFile file = {{0}};
  if (settings_file_open(&file, FILE_NAME, FILE_LEN) != S_SUCCESS) {
    goto cleanup;
  }
  settings_file_set(&file, key, key_len, value, value_len);
  settings_file_close(&file);
cleanup:
  mutex_unlock(s_mutex);
}

bool menu_preferences_get_scroll_wrap_around(void) {
  return s_scroll_wrap_around;
}

void menu_preferences_set_scroll_wrap_around(bool set) {
  s_scroll_wrap_around = set;
  SET_PREF(PREF_KEY_SCROLL_WRAP_AROUND, s_scroll_wrap_around);
}

bool menu_preferences_get_scroll_vibe_on_wrap_around(void) {
    return s_scroll_vibe_on_wrap;
}

void menu_preferences_set_scroll_vibe_on_wrap_around(bool set) {
  s_scroll_vibe_on_wrap = set;
  SET_PREF(PREF_KEY_SCROLL_VIBE_ON_WRAP, s_scroll_vibe_on_wrap);
}

bool menu_preferences_get_scroll_vibe_on_blocked(void) {
  return s_scroll_vibe_on_blocked;
}

void menu_preferences_set_scroll_vibe_on_blocked(bool set) {
  s_scroll_vibe_on_blocked = set;
  SET_PREF(PREF_KEY_SCROLL_VIBE_ON_BLOCKED, s_scroll_vibe_on_blocked);
}
