/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/blob_db/prefs_db.h>
#include <pbl/services/filesystem/pfs.h>
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
#include <fake_spi_flash.h>
#include <fake_system_task.h>

// Stubs
////////////////////////////////////////////////////////////////
#include <stubs_activity.h>
#include <stubs_ambient_light.h>
#include <stubs_analytics.h>
#include <stubs_app_install_manager.h>
#include <stubs_button_lock.h>
#include <stubs_event_loop.h>
#include <stubs_hexdump.h>
#include <stubs_logging.h>
#include <stubs_mfg_info.h>
#include <stubs_mutex.h>
#include <stubs_passert.h>
#include <stubs_pbl_malloc.h>
#include <stubs_rand_ptr.h>
#include <stubs_sleep.h>
#include <stubs_system_theme.h>
#include <stubs_task_wdt.h>
#include <stubs_timeline_peek.h>

extern void shell_prefs_init(void);

void prefs_sync_init(void) {
}

void event_put(PebbleEvent *event) {
}

void i18n_enable(bool enable) {
}

void test_prefs_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
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

// Button lock
////////////////////////////////////////////////////////////////

//! Write a pref the way the phone does: into the backing store, then the blob_db event.
static void prv_phone_write(const char *key, const void *value, size_t len) {
  cl_assert_equal_i(prefs_db_insert((uint8_t *)key, strlen(key), value, len), 0);
  PebbleBlobDBEvent event = (PebbleBlobDBEvent){
    .db_id = BlobDBIdPrefs,
    .type = BlobDBEventTypeInsert,
    .key = (uint8_t *)key,
    .key_len = strlen(key),
  };
  prefs_private_handle_blob_db_event(&event);
}

static uint32_t prv_read_u32(const char *key) {
  uint32_t value;
  cl_assert_equal_i(prefs_db_read((uint8_t *)key, strlen(key), (uint8_t *)&value, sizeof(value)),
                    0);
  return value;
}

void test_prefs_db__button_lock_hold_rejects_invalid_values(void) {
  uint32_t hold_ms = 3000;
  prv_phone_write("buttonLockHoldMs", &hold_ms, sizeof(hold_ms));
  cl_assert_equal_i(shell_prefs_get_button_lock_hold_ms(), 3000);

  // 0 used to mean off, 10 s runs into the PMIC back-button reset.
  const uint32_t invalid[] = {0, 10000, 7};
  for (size_t i = 0; i < ARRAY_LENGTH(invalid); i++) {
    hold_ms = invalid[i];
    prv_phone_write("buttonLockHoldMs", &hold_ms, sizeof(hold_ms));
    cl_assert_equal_i(shell_prefs_get_button_lock_hold_ms(), 2000);
    cl_assert_equal_i(prv_read_u32("buttonLockHoldMs"), 2000);
  }
}

void test_prefs_db__button_lock_auto_rejects_invalid_values(void) {
  uint32_t auto_ms = 30000;
  prv_phone_write("buttonLockAutoMs", &auto_ms, sizeof(auto_ms));
  cl_assert_equal_i(shell_prefs_get_button_lock_auto_ms(), 30000);

  // An unknown duration falls back to off, never to some short timeout.
  auto_ms = 7;
  prv_phone_write("buttonLockAutoMs", &auto_ms, sizeof(auto_ms));
  cl_assert_equal_i(shell_prefs_get_button_lock_auto_ms(), 0);
  cl_assert_equal_i(prv_read_u32("buttonLockAutoMs"), 0);

  ButtonLockAutoScope scope = ButtonLockAutoScopeGeneralUse;
  prv_phone_write("buttonLockAutoScope", &scope, sizeof(scope));
  cl_assert_equal_i(shell_prefs_get_button_lock_auto_scope(), ButtonLockAutoScopeGeneralUse);
  scope = ButtonLockAutoScopeCount;
  prv_phone_write("buttonLockAutoScope", &scope, sizeof(scope));
  cl_assert_equal_i(shell_prefs_get_button_lock_auto_scope(), ButtonLockAutoScopeBoth);
}

void test_prefs_db__button_lock_auto_getters_guard_the_boot_load(void) {
  // The boot load bypasses the handlers, so the getters must not trust the stored values.
  const uint32_t auto_ms = 7;
  const ButtonLockAutoScope scope = ButtonLockAutoScopeCount;
  cl_assert(prefs_private_write_backing((uint8_t *)"buttonLockAutoMs", strlen("buttonLockAutoMs"),
                                        &auto_ms, sizeof(auto_ms)));
  cl_assert(prefs_private_write_backing((uint8_t *)"buttonLockAutoScope",
                                        strlen("buttonLockAutoScope"), &scope, sizeof(scope)));
  shell_prefs_init();
  cl_assert_equal_i(shell_prefs_get_button_lock_auto_ms(), 0);
  cl_assert_equal_i(shell_prefs_get_button_lock_auto_scope(), ButtonLockAutoScopeBoth);
}

void test_prefs_db__button_lock_auto_off_clears_the_pause(void) {
  shell_prefs_set_button_lock_auto_ms(10000);
  shell_prefs_set_button_lock_auto_paused(true);
  shell_prefs_set_button_lock_auto_ms(0);
  cl_assert(!shell_prefs_get_button_lock_auto_paused());
}
