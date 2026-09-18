/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/blob_db/api.h"
#include "pbl/services/blob_db/app_permissions_db.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/util/uuid.h"

#include <string.h>

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_kernel_services_notifications.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

// The convenience setters route through the generic BlobDB API so events fire; here we short
// circuit straight into the implementation and record the calls.
static int s_num_inserts;
static int s_num_deletes;

status_t blob_db_insert(BlobDBId db_id, const uint8_t *key, int key_len, const uint8_t *val,
                        int val_len) {
  cl_assert_equal_i(db_id, BlobDBIdAppPermissions);
  s_num_inserts++;
  return app_permissions_db_insert(key, key_len, val, val_len);
}

status_t blob_db_delete(BlobDBId db_id, const uint8_t *key, int key_len) {
  cl_assert_equal_i(db_id, BlobDBIdAppPermissions);
  s_num_deletes++;
  return app_permissions_db_delete(key, key_len);
}

static const Uuid s_uuid_a = {0x1e, 0xb1, 0xd3, 0x9b, 0x56, 0x98, 0x48, 0x44,
                              0xb3, 0x94, 0x1f, 0x87, 0xb6, 0xbe, 0xae, 0x67};
static const Uuid s_uuid_b = {0xb8, 0x26, 0x2e, 0x08, 0x57, 0xe9, 0x4e, 0x58,
                              0x88, 0x02, 0x45, 0xfd, 0xfe, 0xe0, 0xac, 0x77};

static const AppPermissionsDBEntry s_granted_mic = {
  .version = APP_PERMISSIONS_DB_ENTRY_VERSION,
  .granted_mask = APP_PERMISSION_BIT(AppPermission_Microphone),
  .declared_mask = APP_PERMISSION_BIT(AppPermission_Microphone),
};

static const AppPermissionsDBEntry s_denied_mic = {
  .version = APP_PERMISSIONS_DB_ENTRY_VERSION,
  .granted_mask = 0,
  .declared_mask = APP_PERMISSION_BIT(AppPermission_Microphone),
};

// Setup
////////////////////////////////////////////////////////////////

void test_app_permissions_db__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  app_permissions_db_init();
  s_num_inserts = 0;
  s_num_deletes = 0;
}

void test_app_permissions_db__cleanup(void) {
}

// Tests
////////////////////////////////////////////////////////////////

void test_app_permissions_db__insert_read_roundtrip(void) {
  cl_assert_equal_i(
      S_SUCCESS, app_permissions_db_insert((const uint8_t *)&s_uuid_a, UUID_SIZE,
                                           (const uint8_t *)&s_granted_mic, sizeof(s_granted_mic)));
  cl_assert_equal_i(sizeof(AppPermissionsDBEntry),
                    app_permissions_db_get_len((const uint8_t *)&s_uuid_a, UUID_SIZE));

  AppPermissionsDBEntry entry = {};
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_read((const uint8_t *)&s_uuid_a, UUID_SIZE,
                                                       (uint8_t *)&entry, sizeof(entry)));
  cl_assert_equal_m((void *)&entry, (void *)&s_granted_mic, sizeof(entry));

  // Overwrite with a denial
  cl_assert_equal_i(
      S_SUCCESS, app_permissions_db_insert((const uint8_t *)&s_uuid_a, UUID_SIZE,
                                           (const uint8_t *)&s_denied_mic, sizeof(s_denied_mic)));
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_get(&s_uuid_a, &entry));
  cl_assert_equal_i(entry.granted_mask, 0);
  cl_assert_equal_i(entry.declared_mask, APP_PERMISSION_BIT(AppPermission_Microphone));
}

void test_app_permissions_db__rejects_invalid_records(void) {
  // Non-UUID key
  cl_assert_equal_i(E_INVALID_ARGUMENT, app_permissions_db_insert((const uint8_t *)"abc", 3,
                                                                  (const uint8_t *)&s_granted_mic,
                                                                  sizeof(s_granted_mic)));
  // Short value
  cl_assert_equal_i(
      E_INVALID_ARGUMENT,
      app_permissions_db_insert((const uint8_t *)&s_uuid_a, UUID_SIZE,
                                (const uint8_t *)&s_granted_mic, sizeof(s_granted_mic) - 1));
  // Wrong version
  AppPermissionsDBEntry bad = s_granted_mic;
  bad.version = 7;
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    app_permissions_db_insert((const uint8_t *)&s_uuid_a, UUID_SIZE,
                                              (const uint8_t *)&bad, sizeof(bad)));
  cl_assert_equal_i(0, app_permissions_db_get_len((const uint8_t *)&s_uuid_a, UUID_SIZE));
  cl_assert_equal_i(0, app_permissions_db_get_len((const uint8_t *)"abc", 3));

  AppPermissionsDBEntry entry;
  cl_assert_equal_i(E_DOES_NOT_EXIST, app_permissions_db_get(&s_uuid_a, &entry));
}

void test_app_permissions_db__longer_value_is_truncated_to_known_layout(void) {
  // A newer phone may append fields; we only keep what this firmware understands.
  uint8_t buf[sizeof(AppPermissionsDBEntry) + 4];
  memcpy(buf, &s_granted_mic, sizeof(s_granted_mic));
  memset(buf + sizeof(s_granted_mic), 0xAB, 4);
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_insert((const uint8_t *)&s_uuid_b, UUID_SIZE, buf,
                                                         sizeof(buf)));
  cl_assert_equal_i(sizeof(AppPermissionsDBEntry),
                    app_permissions_db_get_len((const uint8_t *)&s_uuid_b, UUID_SIZE));
  AppPermissionsDBEntry entry;
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_get(&s_uuid_b, &entry));
  cl_assert_equal_m((void *)&entry, (void *)&s_granted_mic, sizeof(entry));
}

void test_app_permissions_db__set_and_delete_go_through_blob_db(void) {
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_set(&s_uuid_a, &s_granted_mic));
  cl_assert_equal_i(1, s_num_inserts);
  AppPermissionsDBEntry entry;
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_get(&s_uuid_a, &entry));

  cl_assert_equal_i(S_SUCCESS, app_permissions_db_delete_for_uuid(&s_uuid_a));
  cl_assert_equal_i(1, s_num_deletes);
  cl_assert_equal_i(E_DOES_NOT_EXIST, app_permissions_db_get(&s_uuid_a, &entry));

  cl_assert_equal_i(E_INVALID_ARGUMENT, app_permissions_db_delete((const uint8_t *)"abc", 3));
}

static bool prv_count_records(const Uuid *uuid, const AppPermissionsDBEntry *entry, void *context) {
  int *count = context;
  (*count)++;
  cl_assert(uuid_equal(uuid, &s_uuid_a) || uuid_equal(uuid, &s_uuid_b));
  cl_assert_equal_i(entry->version, APP_PERMISSIONS_DB_ENTRY_VERSION);
  return true;
}

static bool prv_stop_after_first(const Uuid *uuid, const AppPermissionsDBEntry *entry,
                                 void *context) {
  int *count = context;
  (*count)++;
  return false;
}

void test_app_permissions_db__each(void) {
  int count = 0;
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_each(prv_count_records, &count));
  cl_assert_equal_i(0, count);

  cl_assert_equal_i(S_SUCCESS, app_permissions_db_set(&s_uuid_a, &s_granted_mic));
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_set(&s_uuid_b, &s_denied_mic));
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_each(prv_count_records, &count));
  cl_assert_equal_i(2, count);

  count = 0;
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_each(prv_stop_after_first, &count));
  cl_assert_equal_i(1, count);
}

void test_app_permissions_db__flush(void) {
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_set(&s_uuid_a, &s_granted_mic));
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_flush());
  AppPermissionsDBEntry entry;
  cl_assert_equal_i(E_DOES_NOT_EXIST, app_permissions_db_get(&s_uuid_a, &entry));
  // Still usable after a flush
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_set(&s_uuid_a, &s_denied_mic));
  cl_assert_equal_i(S_SUCCESS, app_permissions_db_get(&s_uuid_a, &entry));
  cl_assert_equal_i(entry.granted_mask, 0);
}
