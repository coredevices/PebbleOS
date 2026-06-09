/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/blob_db/api.h"
#include "pbl/services/blob_db/settings_blob_db.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/notifications/alerts_preferences_private.h"
#include "pbl/services/settings/settings_file.h"
#include "shell/prefs_private.h"
#include "util/attributes.h"

// Fixture
/////////////////////////////////////////////////

// Fakes
/////////////////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_rtc.h"

// Stubs
/////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_bluetooth_persistent_storage.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_mutex.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_session.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"
#include "stubs_powermode_service.h"

void WEAK alerts_preferences_lock(void) { }
void WEAK alerts_preferences_unlock(void) { }
void WEAK alerts_preferences_handle_blob_db_event(PebbleBlobDBEvent *event) { }
void WEAK prefs_private_lock(void) { }
void WEAK prefs_private_unlock(void) { }
void WEAK prefs_private_handle_blob_db_event(PebbleBlobDBEvent *event) { }

// Sync infra we link out: provide stubs that record what the production code
// would have asked sync.c to do.
static int s_sync_record_call_count;
static BlobDBId s_sync_record_db_id;
static int s_sync_record_key_len;
static time_t s_sync_record_last_updated;
static uint8_t s_sync_record_key[SETTINGS_KEY_MAX_LEN];

static int s_sync_db_call_count;

status_t blob_db_sync_record(BlobDBId db_id, const void *key, int key_len, time_t last_updated) {
  s_sync_record_call_count++;
  s_sync_record_db_id = db_id;
  s_sync_record_key_len = key_len;
  s_sync_record_last_updated = last_updated;
  cl_assert(key_len <= (int)sizeof(s_sync_record_key));
  memcpy(s_sync_record_key, key, key_len);
  return S_SUCCESS;
}

status_t blob_db_sync_db(BlobDBId db_id) {
  s_sync_db_call_count++;
  return S_SUCCESS;
}

void test_settings_blob_db__initialize(void) {
  settings_blob_db_reset_for_test();
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(true);
  settings_blob_db_init();
  s_sync_record_call_count = 0;
  s_sync_db_call_count = 0;
  s_sync_record_db_id = (BlobDBId)-1;
  s_sync_record_key_len = 0;
  s_sync_record_last_updated = 0;
}

void test_settings_blob_db__cleanup(void) {
  fake_system_task_callbacks_cleanup();
}

static void prv_store_watch_value(const char *key, const void *val, size_t val_len,
                                  time_t last_updated) {
  SettingsFile file;
  cl_must_pass(settings_file_open(&file, "notifpref", 1024));
  cl_must_pass(settings_file_set_with_timestamp(&file, key, strlen(key), val, val_len,
                                                last_updated));
  settings_file_close(&file);
}

void test_settings_blob_db__stale_phone_write_enqueues_writeback(void) {
  const char *key = "dndManuallyEnabled";
  const bool watch_value = true;
  const time_t watch_ts = 100;
  prv_store_watch_value(key, &watch_value, sizeof(watch_value), watch_ts);

  // Phone pushes a stale value (older timestamp) for the same key.
  const bool phone_value = false;
  const time_t phone_ts = 50;
  status_t rv = settings_blob_db_insert_with_timestamp(
      (const uint8_t *)key, strlen(key), (const uint8_t *)&phone_value, sizeof(phone_value), phone_ts);
  cl_assert_equal_i(rv, E_INVALID_OPERATION);

  // The reject path should have enqueued a writeback of the watch's value.
  cl_assert_equal_i(s_sync_record_call_count, 1);
  cl_assert_equal_i(s_sync_record_db_id, BlobDBIdSettings);
  cl_assert_equal_i(s_sync_record_key_len, (int)strlen(key));
  cl_assert_equal_i(s_sync_record_last_updated, watch_ts);
  cl_assert_equal_m(s_sync_record_key, key, strlen(key));

  // The watch's stored value must not have been overwritten.
  SettingsFile file;
  cl_must_pass(settings_file_open(&file, "notifpref", 1024));
  bool stored;
  cl_must_pass(settings_file_get(&file, key, strlen(key), &stored, sizeof(stored)));
  cl_assert_equal_b(stored, true);
  settings_file_close(&file);
}

void test_settings_blob_db__fresh_phone_write_does_not_enqueue_writeback(void) {
  const char *key = "dndManuallyEnabled";
  const bool watch_value = true;
  const time_t watch_ts = 50;
  prv_store_watch_value(key, &watch_value, sizeof(watch_value), watch_ts);

  // Phone pushes a newer value than what the watch has.
  const bool phone_value = false;
  const time_t phone_ts = 100;
  status_t rv = settings_blob_db_insert_with_timestamp(
      (const uint8_t *)key, strlen(key), (const uint8_t *)&phone_value, sizeof(phone_value), phone_ts);
  cl_assert_equal_i(rv, S_SUCCESS);

  // Fresh writes must not trigger a writeback (they originated from the phone).
  cl_assert_equal_i(s_sync_record_call_count, 0);
}

void test_settings_blob_db__phone_write_to_unknown_key_does_not_enqueue_writeback(void) {
  // A phone-originated insert for a key the watch doesn't have should be
  // rejected by the whitelist without enqueuing a writeback.
  const char *key = "notARealKey";
  const uint8_t val = 1;
  status_t rv = settings_blob_db_insert_with_timestamp(
      (const uint8_t *)key, strlen(key), &val, sizeof(val), 100);
  cl_assert_equal_i(rv, E_INVALID_OPERATION);
  cl_assert_equal_i(s_sync_record_call_count, 0);
}
