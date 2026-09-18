/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/event_service_client.h"
#include "kernel/events.h"
#include "pbl/services/app_permissions/app_permissions.h"
#include "pbl/services/blob_db/api.h"
#include "pbl/services/blob_db/app_permissions_db.h"
#include "process_management/app_install_manager.h"
#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"

#include <string.h>

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_events.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

// In-memory single-record fake of the AppPermissions BlobDB
////////////////////////////////////////////////////////////////

static bool s_has_record;
static Uuid s_record_uuid;
static AppPermissionsDBEntry s_record;
static int s_num_sets;

status_t app_permissions_db_get(const Uuid *uuid, AppPermissionsDBEntry *entry_out) {
  if (!s_has_record || !uuid_equal(uuid, &s_record_uuid)) {
    return E_DOES_NOT_EXIST;
  }
  *entry_out = s_record;
  return S_SUCCESS;
}

status_t app_permissions_db_set(const Uuid *uuid, const AppPermissionsDBEntry *entry) {
  s_has_record = true;
  s_record_uuid = *uuid;
  s_record = *entry;
  s_num_sets++;
  return S_SUCCESS;
}

// Current app
////////////////////////////////////////////////////////////////

static PebbleProcessMd s_md;
static const PebbleProcessMd *s_current_md;
static AppInstallId s_current_id = 1;

const PebbleProcessMd *app_manager_get_current_app_md(void) {
  return s_current_md;
}

AppInstallId app_manager_get_current_app_id(void) {
  return s_current_id;
}

bool app_install_id_from_system(AppInstallId id) {
  return (id < INSTALL_ID_INVALID);
}

// Event service
////////////////////////////////////////////////////////////////

static EventServiceInfo *s_blob_db_subscription;

void event_service_client_subscribe(EventServiceInfo *info) {
  cl_assert_equal_i(info->type, PEBBLE_BLOBDB_EVENT);
  s_blob_db_subscription = info;
}

void event_service_client_unsubscribe(EventServiceInfo *info) {
}

static void prv_send_blob_db_event(BlobDBId db_id, BlobDBEventType type, const Uuid *key) {
  PebbleEvent event = {
    .type = PEBBLE_BLOBDB_EVENT,
    .blob_db = {
      .db_id = db_id,
      .type = type,
      .key = (uint8_t *)key,
      .key_len = key ? UUID_SIZE : 0,
    },
  };
  s_blob_db_subscription->handler(&event, s_blob_db_subscription->context);
}

static const Uuid s_uuid_app = {0x1e, 0xb1, 0xd3, 0x9b, 0x56, 0x98, 0x48, 0x44,
                                0xb3, 0x94, 0x1f, 0x87, 0xb6, 0xbe, 0xae, 0x67};
static const Uuid s_uuid_other = {0xb8, 0x26, 0x2e, 0x08, 0x57, 0xe9, 0x4e, 0x58,
                                  0x88, 0x02, 0x45, 0xfd, 0xfe, 0xe0, 0xac, 0x77};

static void prv_set_record(const Uuid *uuid, AppPermissionMask granted) {
  s_has_record = true;
  s_record_uuid = *uuid;
  s_record = (AppPermissionsDBEntry){
    .version = APP_PERMISSIONS_DB_ENTRY_VERSION,
    .granted_mask = granted,
    .declared_mask = APP_PERMISSION_BIT(AppPermission_Microphone),
  };
}

// Setup
////////////////////////////////////////////////////////////////

void test_app_permissions__initialize(void) {
  fake_event_init();
  s_has_record = false;
  s_num_sets = 0;
  s_md = (PebbleProcessMd){.uuid = s_uuid_app, .uses_microphone = true, .is_unprivileged = true};
  s_current_md = &s_md;
  s_current_id = 1;
  s_blob_db_subscription = NULL;
  app_permissions_init();
  cl_assert(s_blob_db_subscription != NULL);
}

void test_app_permissions__cleanup(void) {
}

// Tests
////////////////////////////////////////////////////////////////

void test_app_permissions__state_matrix(void) {
  // Not declared beats everything
  prv_set_record(&s_uuid_app, APP_PERMISSION_BIT(AppPermission_Microphone));
  cl_assert_equal_i(
      AppPermissionStateNotDeclared,
      app_permissions_get_state_for_app(&s_uuid_app, false, AppPermission_Microphone));

  // Declared + granted record
  cl_assert_equal_i(AppPermissionStateGranted,
                    app_permissions_get_state_for_app(&s_uuid_app, true, AppPermission_Microphone));

  // Declared + record without the bit
  prv_set_record(&s_uuid_app, 0);
  cl_assert_equal_i(AppPermissionStateDenied,
                    app_permissions_get_state_for_app(&s_uuid_app, true, AppPermission_Microphone));

  // Declared + no record: fail closed
  s_has_record = false;
  cl_assert_equal_i(AppPermissionStateDenied,
                    app_permissions_get_state_for_app(&s_uuid_app, true, AppPermission_Microphone));

  // Out of range permission
  cl_assert_equal_i(AppPermissionStateNotDeclared,
                    app_permissions_get_state_for_app(&s_uuid_app, true, AppPermissionCount));
}

void test_app_permissions__current_app(void) {
  // No app running
  s_current_md = NULL;
  cl_assert_equal_i(AppPermissionStateNotDeclared,
                    app_permissions_get_state_for_current_app(AppPermission_Microphone));
  s_current_md = &s_md;

  // Declared, no record
  cl_assert_equal_i(AppPermissionStateDenied,
                    app_permissions_get_state_for_current_app(AppPermission_Microphone));
  cl_assert_equal_b(false, app_permissions_is_granted_for_current_app(AppPermission_Microphone));

  // Granted
  prv_set_record(&s_uuid_app, APP_PERMISSION_BIT(AppPermission_Microphone));
  cl_assert_equal_b(true, app_permissions_is_granted_for_current_app(AppPermission_Microphone));

  // A record for a different app does not count
  prv_set_record(&s_uuid_other, APP_PERMISSION_BIT(AppPermission_Microphone));
  cl_assert_equal_b(false, app_permissions_is_granted_for_current_app(AppPermission_Microphone));

  // A watchface never has it, even when declared and granted
  s_md.process_type = ProcessTypeWatchface;
  cl_assert_equal_i(AppPermissionStateNotDeclared,
                    app_permissions_get_state_for_current_app(AppPermission_Microphone));
  s_md.process_type = ProcessTypeApp;

  // Not declared in the header
  s_md.uses_microphone = false;
  prv_set_record(&s_uuid_app, APP_PERMISSION_BIT(AppPermission_Microphone));
  cl_assert_equal_i(AppPermissionStateNotDeclared,
                    app_permissions_get_state_for_current_app(AppPermission_Microphone));
}

void test_app_permissions__system_apps_always_granted(void) {
  s_md.uses_microphone = false;
  s_current_id = -5; // system install ids are negative
  cl_assert_equal_i(AppPermissionStateGranted,
                    app_permissions_get_state_for_current_app(AppPermission_Microphone));
  // Built-in apps launched without an install id (e.g. from the console) are privileged
  s_current_id = 0;
  s_md.is_unprivileged = false;
  cl_assert_equal_i(AppPermissionStateGranted,
                    app_permissions_get_state_for_current_app(AppPermission_Microphone));
}

void test_app_permissions__set_granted(void) {
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    app_permissions_set_granted(&s_uuid_app, AppPermissionCount, true));
  cl_assert_equal_i(E_INVALID_ARGUMENT,
                    app_permissions_set_granted(NULL, AppPermission_Microphone, true));

  // Creates a record when none exists
  cl_assert_equal_i(S_SUCCESS,
                    app_permissions_set_granted(&s_uuid_app, AppPermission_Microphone, true));
  cl_assert_equal_i(1, s_num_sets);
  cl_assert(uuid_equal(&s_record_uuid, &s_uuid_app));
  cl_assert_equal_i(s_record.version, APP_PERMISSIONS_DB_ENTRY_VERSION);
  cl_assert_equal_i(s_record.granted_mask, APP_PERMISSION_BIT(AppPermission_Microphone));
  cl_assert_equal_i(s_record.declared_mask, APP_PERMISSION_BIT(AppPermission_Microphone));

  // Revoke keeps the declared bit
  cl_assert_equal_i(S_SUCCESS,
                    app_permissions_set_granted(&s_uuid_app, AppPermission_Microphone, false));
  cl_assert_equal_i(s_record.granted_mask, 0);
  cl_assert_equal_i(s_record.declared_mask, APP_PERMISSION_BIT(AppPermission_Microphone));
}

void test_app_permissions__blob_db_event_notifies_current_app(void) {
  // Other databases are ignored
  prv_send_blob_db_event(BlobDBIdApps, BlobDBEventTypeInsert, &s_uuid_app);
  cl_assert_equal_i(0, fake_event_get_count());

  // Another app's record is ignored
  prv_set_record(&s_uuid_other, APP_PERMISSION_BIT(AppPermission_Microphone));
  prv_send_blob_db_event(BlobDBIdAppPermissions, BlobDBEventTypeInsert, &s_uuid_other);
  cl_assert_equal_i(0, fake_event_get_count());

  // Our record: Granted
  prv_set_record(&s_uuid_app, APP_PERMISSION_BIT(AppPermission_Microphone));
  prv_send_blob_db_event(BlobDBIdAppPermissions, BlobDBEventTypeInsert, &s_uuid_app);
  cl_assert_equal_i(1, fake_event_get_count());
  PebbleEvent e = fake_event_get_last();
  cl_assert_equal_i(e.type, PEBBLE_APP_PERMISSION_EVENT);
  cl_assert_equal_i(e.app_permission.permission, AppPermission_Microphone);
  cl_assert_equal_i(e.app_permission.state, AppPermissionStateGranted);

  // Delete: back to Denied
  s_has_record = false;
  prv_send_blob_db_event(BlobDBIdAppPermissions, BlobDBEventTypeDelete, &s_uuid_app);
  cl_assert_equal_i(2, fake_event_get_count());
  e = fake_event_get_last();
  cl_assert_equal_i(e.app_permission.state, AppPermissionStateDenied);

  // Flush affects everyone
  prv_send_blob_db_event(BlobDBIdAppPermissions, BlobDBEventTypeFlush, NULL);
  cl_assert_equal_i(3, fake_event_get_count());

  // Undeclared apps never hear about it
  s_md.uses_microphone = false;
  prv_send_blob_db_event(BlobDBIdAppPermissions, BlobDBEventTypeInsert, &s_uuid_app);
  cl_assert_equal_i(3, fake_event_get_count());

  // No app running
  s_current_md = NULL;
  prv_send_blob_db_event(BlobDBIdAppPermissions, BlobDBEventTypeFlush, NULL);
  cl_assert_equal_i(3, fake_event_get_count());
}
