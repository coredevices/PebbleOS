/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_region/flash_region.h"
#include "syscall/syscall.h"
#include "services/normal/wakeup.h"
#include "services/common/event_service.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/settings/settings_file.h"
#include "process_management/app_install_manager.h"
#include "util/attributes.h"

#include "clar.h"

// Fakes
//////////////////////////////////////////////////////////
#include "fake_app_manager.h"
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_time.h"

// Stubs
//////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_events.h"
#include "stubs_logging.h"
#include "stubs_print.h"
#include "stubs_serial.h"
#include "stubs_passert.h"
#include "stubs_sleep.h"
#include "stubs_mutex.h"
#include "stubs_hexdump.h"
#include "stubs_pebble_process_md.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_task_watchdog.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_memory_layout.h"

#define SETTINGS_FILE_NAME  "wakeup"
#define SETTINGS_FILE_SIZE  2048

#define WAKEUP_REASON       0x1337
#define TIMESTAMP           1337

// Structures
////////////////////////////////////

typedef struct PACKED {
  Uuid uuid;
  int32_t reason;
  bool repeating;
  uint16_t repeat_hours_missed;
  bool notify_if_missed;
} WakeupEntryV1;

typedef struct PACKED {
  Uuid uuid;
  int32_t reason;
  bool repeating;
  uint16_t repeat_hours_missed;
  bool notify_if_missed;
  time_t timestamp;
  bool utc;
} WakeupEntryV2;

// Globals
////////////////////////////////////

static const Uuid app_uuid = (Uuid) {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5
};

static PebbleProcessMd s_test_app_md = {
  .uuid = (Uuid) {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5
  }
};

static AppInstallEntry s_app_install_entry = {
  .install_id = 1,
};

static WakeupEntryV1 w_entry;

// Local Stubs
////////////////////////////////////
void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback start_cb,
    EventServiceRemoveSubscriberCallback stop_cb) {
  return;
}

void wakeup_popup_window(uint8_t missed_apps_count, uint8_t *missed_apps_banks) {
  return;
}

bool app_install_get_entry_from_install_id(const AppInstallId id, AppInstallEntry *entry) {
  *entry = s_app_install_entry;
  return true;
}

bool clock_is_timezone_set(void) {
  return false;
}

// Helpers
////////////////////////////////////

void open_settings_file(SettingsFile *file) {
  cl_must_pass(settings_file_open(file, SETTINGS_FILE_NAME, SETTINGS_FILE_SIZE));
}

void close_settings_file(SettingsFile *file) {
  settings_file_close(file);
}

// Tests
////////////////////////////////////

void test_migrate_wakeup__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);

  // Load pre-migration wakeup entry onto pfs
  SettingsFile file;
  const int32_t timestamp = TIMESTAMP;

  // Create the Migration Entry
  w_entry = (WakeupEntryV1) {
    .uuid = app_uuid,
    .reason = WAKEUP_REASON,
    .repeating = false,
    .repeat_hours_missed = 0,
    .notify_if_missed = true
  };

  open_settings_file(&file);

  cl_must_pass(settings_file_set(&file, (uint8_t*)&timestamp, sizeof(timestamp),
      (uint8_t*)&w_entry, sizeof(w_entry)));

  close_settings_file(&file);
}

void test_migrate_wakeup__test_migration_of_wakeup_entries(void) {
  SettingsFile file;
  WakeupId wakeup_id = TIMESTAMP;
  WakeupEntryV1 wakeup_entry_v1;

  open_settings_file(&file);

  cl_must_pass(settings_file_get(&file, (uint8_t*)&wakeup_id, sizeof(wakeup_id),
      (uint8_t*)&wakeup_entry_v1, sizeof(wakeup_entry_v1)));
  cl_assert_equal_i(wakeup_entry_v1.reason, WAKEUP_REASON);
  cl_assert_equal_i(wakeup_entry_v1.repeat_hours_missed, 0);
  cl_assert(!wakeup_entry_v1.repeating);
  cl_assert(wakeup_entry_v1.notify_if_missed);
  uuid_equal(&wakeup_entry_v1.uuid, &app_uuid);

  close_settings_file(&file);

  // Migrate the timezone and check that the new entry is correct and the second version
  // of a Wakeup Entry
  wakeup_init();

  WakeupEntryV2 wakeup_entry_v2;

  open_settings_file(&file);

  cl_must_pass(settings_file_exists(&file, (uint8_t*)&wakeup_id, sizeof(wakeup_id)));
  cl_assert_equal_i(settings_file_get_len(&file, (uint8_t*)&wakeup_id, sizeof(wakeup_id)),
      sizeof(WakeupEntryV2));
  cl_must_pass(settings_file_get(&file, (uint8_t*)&wakeup_id, sizeof(wakeup_id),
      (uint8_t*)&wakeup_entry_v2, sizeof(wakeup_entry_v2)));

  cl_assert_equal_i(wakeup_entry_v2.reason, wakeup_entry_v1.reason);
  cl_assert_equal_i(wakeup_entry_v2.repeat_hours_missed, wakeup_entry_v1.repeat_hours_missed);
  cl_assert_equal_i(wakeup_entry_v2.timestamp, (int32_t)TIMESTAMP);
  cl_assert(wakeup_entry_v2.utc == false);
  cl_assert(wakeup_entry_v2.repeating == wakeup_entry_v1.repeating);
  cl_assert(wakeup_entry_v2.notify_if_missed == wakeup_entry_v1.notify_if_missed);
  uuid_equal(&wakeup_entry_v2.uuid, &wakeup_entry_v2.uuid);

  close_settings_file(&file);
}
