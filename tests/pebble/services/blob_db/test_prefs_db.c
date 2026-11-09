/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>
#include <pbl/services/blob_db/prefs_db.h>
#include <pbl/util/size.h>
#include <pbl/util/uuid.h>

#include <clar.h>
#include <shell/prefs.h>
#include <shell/prefs_private.h>

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include <fake_kernel_services_notifications.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>
#include <fake_sys_work.h>

// Stubs
////////////////////////////////////////////////////////////////
#include <stubs_dark_mode_updater.h>

void prefs_sync_init(void) {
}

void event_put(PebbleEvent *event) {
}

void language_pack_enable(bool enable) {
}

void test_prefs_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
}

void test_prefs_db__cleanup(void) {
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

static void prv_insert_from_phone(const char *key, const void *value, size_t value_len) {
  const int key_len = strlen(key);
  cl_assert_equal_i(prefs_db_insert((uint8_t *)key, key_len, (uint8_t *)value, value_len), 0);
  PebbleBlobDBEvent event = (PebbleBlobDBEvent){
    .db_id = BlobDBIdPrefs,
    .type = BlobDBEventTypeInsert,
    .key = (uint8_t *)key,
    .key_len = key_len,
  };
  prefs_private_handle_blob_db_event(&event);
}

void test_prefs_db__dark_mode(void) {
  shell_prefs_set_dark_mode(DarkModeScheduled);
  cl_assert_equal_i(shell_prefs_get_dark_mode(), DarkModeScheduled);

  const uint8_t on = DarkModeOn;
  prv_insert_from_phone("darkMode", &on, sizeof(on));
  cl_assert_equal_i(shell_prefs_get_dark_mode(), DarkModeOn);

  // An out of range mode falls back to Off, also in the backing store
  const uint8_t invalid = DarkModeCount;
  prv_insert_from_phone("darkMode", &invalid, sizeof(invalid));
  cl_assert_equal_i(shell_prefs_get_dark_mode(), DarkModeOff);
  uint8_t stored;
  cl_assert_equal_i(prefs_db_read((uint8_t *)"darkMode", strlen("darkMode"), &stored, 1), 0);
  cl_assert_equal_i(stored, DarkModeOff);
}

void test_prefs_db__dark_mode_schedule(void) {
  const DarkModeSchedule schedule = {
    .from_hour = 22,
    .from_minute = 30,
    .to_hour = 6,
    .to_minute = 15,
  };
  shell_prefs_set_dark_mode_schedule(&schedule);
  DarkModeSchedule read;
  shell_prefs_get_dark_mode_schedule(&read);
  cl_assert_equal_m(&read, &schedule, sizeof(schedule));

  // Any out of range field resets the whole schedule to the default
  const DarkModeSchedule invalid[] = {
    {.from_hour = 24},
    {.from_minute = 60},
    {.to_hour = 24},
    {.to_minute = 60},
  };
  const DarkModeSchedule expected = {.from_hour = 19, .from_minute = 0, .to_hour = 7};
  for (size_t i = 0; i < ARRAY_LENGTH(invalid); i++) {
    shell_prefs_set_dark_mode_schedule(&schedule);
    prv_insert_from_phone("darkModeSchedule", &invalid[i], sizeof(invalid[i]));
    shell_prefs_get_dark_mode_schedule(&read);
    cl_assert_equal_m(&read, &expected, sizeof(expected));
  }
}
