/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"
#include "pebble_asserts.h"

#include "apps/system_app_ids.h"
#include "flash_region/flash_region.h"
#include "process_management/app_install_manager.h"
#include "services/normal/process_management/app_storage.h"
#include "process_management/pebble_process_info.h"
#include "process_management/pebble_process_md.h"
#include "resource/resource.h"
#include "resource/resource_storage.h"
#include "resource/resource_storage_file.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/app_cache.h"
#include "services/normal/blob_db/app_db.h"
#include "util/build_id.h"
#include "util/time/time.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fixtures/load_test_resources.h"

// Stub Includes
////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_bootbits.h"
#include "stubs_event_service_client.h"
#include "stubs_events.h"
#include "stubs_heap.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_persist.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"
#include "stubs_watchface.h"
#include "stubs_worker_manager.h"

// Fake Includes
////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_rtc.h"

// Stubs
////////////////////////////////////

const uint32_t g_num_file_resource_stores = 0;
const FileResourceData g_file_resource_stores[] = {};

void quick_launch_remove_app(const Uuid *uuid) {
  return;
}

bool build_id_contains_gnu_build_id(const ElfExternalNote *note) {
  return false;
}

const char *app_custom_get_title(AppInstallId app_id) {
  return "";
}

status_t pin_db_delete_with_parent(const TimelineItemId *parent_id) {
  return S_SUCCESS;
}

bool pin_db_exists_with_parent(const TimelineItemId *parent_id) {
  return true;
}

AppInstallId quick_launch_get_app(uint8_t button) {
  return 0;
}

AppInstallId worker_preferences_get_default_worker(void) {
  return 0;
}

bool app_fetch_in_progress(void) {
  return false;
}

void app_fetch_cancel_from_system_task(void) {
}

void comm_session_app_session_capabilities_evict(const Uuid *app_uuid) {
}

void put_bytes_cancel(void) {
}

// Fakes
////////////////////////////////////
uint32_t time_get_uptime_seconds(void) {
  return rtc_get_time();
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  callback(data);
}

bool system_task_add_callback(void (*cb)(void*), void *data) {
  cb(data);
  return true;
}

#define APP_REGISTRY_FIXTURE_PATH "app_registry"

#define APP1_APP_FIXTURE_NAME "feature-background-counter-app"
#define APP1_WORKER_FIXTURE_NAME "feature-background-counter-worker"
#define APP1_RESOURCES_FIXTURE_NAME "feature-background-counter.pbpack"

#define APP2_APP_FIXTURE_NAME "feature_menu_layer"
#define APP2_RESOURCES_FIXTURE_NAME "feature_menu_layer.pbpack"

#define BACKGROUND_COUNTER_APP_NAME "Background Counter"
#define MENU_LAYER_APP_NAME "MenuLayerName"

void load_fixture_on_pfs(const char *name, const char *pfs_name) {
  char res_path[strlen(CLAR_FIXTURE_PATH) + strlen(APP_REGISTRY_FIXTURE_PATH) + strlen(name) + 3];
  sprintf(res_path, "%s/%s/%s", CLAR_FIXTURE_PATH, APP_REGISTRY_FIXTURE_PATH, name);
  // Check that file exists and fits in buffer
  struct stat st;
  cl_assert(stat(res_path, &st) == 0);

  FILE *file = fopen(res_path, "r");
  cl_assert(file);

  uint8_t buf[st.st_size];
  // copy file to fake flash storage
  cl_assert(fread(buf, 1, st.st_size, file) > 0);

  int fd = pfs_open(pfs_name, OP_FLAG_WRITE, FILE_TYPE_STATIC, st.st_size);
  cl_assert(fd >= 0);
  cl_assert(st.st_size == pfs_write(fd, buf, st.st_size));
  pfs_close(fd);
}


/* Start of test */

static const AppInstallId CRAZY_ID = 171717;

bool app_install_get_entry_for_uuid(const Uuid *uuid, AppInstallEntry *entry) {
  AppInstallId id = app_install_get_id_for_uuid(uuid);
  return app_install_get_entry_for_install_id(id, entry);
}


static bool prv_app_install_is_watchface(AppInstallId id) {
  AppInstallEntry entry;
  bool exists = app_install_get_entry_for_install_id(id, &entry);
  if (!exists) {
    return false;
  }
  return app_install_entry_is_watchface(&entry);
}

bool app_install_has_worker(AppInstallId id) {
  AppInstallEntry entry;
  bool exists = app_install_get_entry_for_install_id(id, &entry);
  if (!exists) {
    return false;
  }
  return app_install_entry_has_worker(&entry);
}

bool app_install_is_hidden(AppInstallId id) {
  AppInstallEntry entry;
  bool exists = app_install_get_entry_for_install_id(id, &entry);
  if (!exists) {
    return false;
  }
  return app_install_entry_is_hidden(&entry);
}

bool app_install_entries_equal(AppInstallEntry *one, AppInstallEntry *two) {
  bool id = (one->install_id == two->install_id);
  bool type = (one->type == two->type);
  bool visibility = (one->visibility == two->visibility);
  bool process_type = (one->process_type == two->process_type);
  bool uuid = !memcmp(&one->uuid, &two->uuid, sizeof(Uuid));
  bool name = !strcmp(one->name,two->name);
  bool icon = (one->icon_resource_id == two->icon_resource_id);

  return (id && type && visibility && process_type && uuid && name && icon);
}

// background counter
static const uint32_t bg_counter_size = (1132 + 276 + 4092);
static const AppDBEntry bg_counter = {
  .name = BACKGROUND_COUNTER_APP_NAME,
  .uuid = {0x1e, 0xb1, 0xd3, 0x9b, 0x56, 0x98, 0x48, 0x44,
           0xb3, 0x94, 0x1f, 0x87, 0xb6, 0xbe, 0xae, 0x67},
  .info_flags = PROCESS_INFO_HAS_WORKER | PROCESS_INFO_STANDARD_APP,
  .app_version = {
    .major = 1,
    .minor = 0,
  },
  .sdk_version = {
    .major = 5,
    .minor = 13,
  },
  .app_face_bg_color = {0},
  .template_id = 0,
  .icon_resource_id = 0,
};

// menu layer
static const uint32_t menu_layer_size = (1140 + 7852);
static const AppDBEntry menu_layer = {
  .name = MENU_LAYER_APP_NAME,
  .uuid = {0xb8, 0x26, 0x2e, 0x08, 0x57, 0xe9, 0x4e, 0x58,
           0x88, 0x02, 0x45, 0xfd, 0xfe, 0xe0, 0xac, 0x77},
  .info_flags = PROCESS_INFO_STANDARD_APP,
  .app_version = {
    .major = 2,
    .minor = 0,
  },
  .sdk_version = {
    .major = 5,
    .minor = 13,
  },
  .app_face_bg_color = {0},
  .template_id = 0,
  .icon_resource_id = 0,
};


static const Uuid tictoc_uuid = { 0x8f, 0x3c, 0x86, 0x86, 0x31, 0xa1, 0x4f, 0x5f,
                                  0x91, 0xf5, 0x01, 0x60, 0x0c, 0x9b, 0xdc, 0x59 };

static const Uuid music_uuid = {0x1f, 0x03, 0x29, 0x3d, 0x47, 0xaf, 0x4f, 0x28,
                                0xb9, 0x60, 0xf2, 0xb0, 0x2a, 0x6d, 0xd7, 0x57};

static const Uuid sports_uuid = {0x4d, 0xab, 0x81, 0xa6, 0xd2, 0xfc, 0x45, 0x8a,
                                 0x99, 0x2c, 0x7a, 0x1f, 0x3b, 0x96, 0xa9, 0x70};

AppInstallId tictoc_id;
AppInstallId music_id;
AppInstallId sports_id;
AppInstallId bg_counter_id;
AppInstallId menu_layer_id;


void test_app_install_manager__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);

  app_install_manager_init();
  app_db_init();
  app_db_flush();

  app_cache_init();
  app_cache_flush();

  tictoc_id = app_install_get_id_for_uuid(&tictoc_uuid);
  music_id = app_install_get_id_for_uuid(&music_uuid);
  sports_id = app_install_get_id_for_uuid(&sports_uuid);

  cl_assert_equal_i(-69, tictoc_id);
  cl_assert_equal_i(-3, music_id);
  cl_assert_equal_i(-53, sports_id);

  // load system resources
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, true);
  resource_init();

  // simulate installing bg_counter on flash
  app_db_insert((uint8_t *)&bg_counter.uuid, sizeof(Uuid),
                (uint8_t *)&bg_counter, sizeof(AppDBEntry));
  bg_counter_id = app_db_get_install_id_for_uuid(&bg_counter.uuid);
  app_cache_add_entry(bg_counter_id, bg_counter_size /* size */);
  cl_assert_equal_i(1, bg_counter_id);

  // load first app
  char filename_buf[32];
  app_storage_get_file_name(filename_buf, sizeof(filename_buf), 1,
                            PebbleTask_App);
  load_fixture_on_pfs(APP1_APP_FIXTURE_NAME, filename_buf);
  app_storage_get_file_name(filename_buf, sizeof(filename_buf), 1,
                            PebbleTask_Worker);
  load_fixture_on_pfs(APP1_WORKER_FIXTURE_NAME, filename_buf);
  resource_storage_get_file_name(filename_buf, sizeof(filename_buf), 1);
  load_fixture_on_pfs(APP1_RESOURCES_FIXTURE_NAME, filename_buf);

  // simulate installing app2 on flash
  app_db_insert((uint8_t *)&menu_layer.uuid, sizeof(Uuid), (uint8_t *)&menu_layer, sizeof(AppDBEntry));
  menu_layer_id = app_db_get_install_id_for_uuid(&menu_layer.uuid);
  app_cache_add_entry(menu_layer_id, menu_layer_size /* size */);
  cl_assert_equal_i(2, menu_layer_id);

  // load second app
  app_storage_get_file_name(filename_buf, sizeof(filename_buf), 2,
                            PebbleTask_App);
  load_fixture_on_pfs(APP2_APP_FIXTURE_NAME, filename_buf);
  resource_storage_get_file_name(filename_buf, sizeof(filename_buf), 2);
  load_fixture_on_pfs(APP2_RESOURCES_FIXTURE_NAME, filename_buf);
}

void test_app_install_manager__cleanup(void) {

}

/*************************************

 *************************************/


void test_app_install_manager__get_id_invalid_uuid(void) {
  const Uuid made_up = {0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
                             0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17};
  cl_assert_equal_i(INSTALL_ID_INVALID, app_install_get_id_for_uuid(&made_up));
  cl_assert_equal_i(INSTALL_ID_INVALID, app_install_get_id_for_uuid(&UUID_INVALID));
  cl_assert_equal_i(INSTALL_ID_INVALID, app_install_get_id_for_uuid(&(const Uuid)UUID_SYSTEM));
}

void test_app_install_manager__compare_app_entry_retrieve_methods(void) {
  AppInstallEntry id_entry;
  AppInstallEntry uuid_entry;

  cl_assert_equal_b(true, app_install_get_entry_for_install_id(tictoc_id, &id_entry));
  cl_assert_equal_b(true, app_install_get_entry_for_uuid(&tictoc_uuid, &uuid_entry));
  cl_assert_equal_b(true, app_install_entries_equal(&id_entry, &uuid_entry));

  cl_assert_equal_b(true, app_install_get_entry_for_install_id(music_id, &id_entry));
  cl_assert_equal_b(true, app_install_get_entry_for_uuid(&music_uuid, &uuid_entry));
  cl_assert_equal_b(true, app_install_entries_equal(&id_entry, &uuid_entry));

  cl_assert_equal_b(true, app_install_get_entry_for_install_id(sports_id, &id_entry));
  cl_assert_equal_b(true, app_install_get_entry_for_uuid(&sports_uuid, &uuid_entry));
  cl_assert_equal_b(true, app_install_entries_equal(&id_entry, &uuid_entry));

  cl_assert_equal_b(true, app_install_get_entry_for_install_id(bg_counter_id, &id_entry));
  cl_assert_equal_b(true, app_install_get_entry_for_uuid(&bg_counter.uuid, &uuid_entry));
  cl_assert_equal_b(true, app_install_entries_equal(&id_entry, &uuid_entry));

  cl_assert_equal_b(true, app_install_get_entry_for_install_id(menu_layer_id, &id_entry));
  cl_assert_equal_b(true, app_install_get_entry_for_uuid(&menu_layer.uuid, &uuid_entry));
  cl_assert_equal_b(true, app_install_entries_equal(&id_entry, &uuid_entry));
}

void test_app_install_manager__is_watchface_via_install_id(void) {
  cl_assert_equal_b(true,  app_install_is_watchface(tictoc_id));
  cl_assert_equal_b(false, app_install_is_watchface(music_id));
  cl_assert_equal_b(false, app_install_is_watchface(sports_id));
  cl_assert_equal_b(false, app_install_is_watchface(bg_counter_id));
  cl_assert_equal_b(false, app_install_is_watchface(menu_layer_id));

  cl_assert_equal_b(false, app_install_is_watchface(CRAZY_ID));
}

void test_app_install_manager__is_watchface_via_entry(void) {
  cl_assert_equal_b(true,  prv_app_install_is_watchface(tictoc_id));
  cl_assert_equal_b(false, prv_app_install_is_watchface(music_id));
  cl_assert_equal_b(false, prv_app_install_is_watchface(sports_id));
  cl_assert_equal_b(false, prv_app_install_is_watchface(bg_counter_id));
  cl_assert_equal_b(false, prv_app_install_is_watchface(menu_layer_id));

  cl_assert_equal_b(false, prv_app_install_is_watchface(CRAZY_ID));
}

void test_app_install_manager__get_uuid_for_install_id(void) {
  Uuid uuid = {};
  cl_assert_equal_b(false, app_install_get_uuid_for_install_id(INSTALL_ID_INVALID, &uuid));
  cl_assert_equal_uuid(uuid, UUID_INVALID);
  cl_assert_equal_b(true,  app_install_get_uuid_for_install_id(tictoc_id, &uuid));
  cl_assert_equal_uuid(uuid, tictoc_uuid);
  cl_assert_equal_b(true,  app_install_get_uuid_for_install_id(music_id, &uuid));
  cl_assert_equal_uuid(uuid, music_uuid);
  cl_assert_equal_b(true,  app_install_get_uuid_for_install_id(sports_id, &uuid));
  cl_assert_equal_uuid(uuid, sports_uuid);
  cl_assert_equal_b(false, app_install_get_uuid_for_install_id(CRAZY_ID, &uuid));
  cl_assert_equal_uuid(uuid, UUID_INVALID);
}

void test_app_install_manager__has_worker(void) {
  cl_assert_equal_b(false, app_install_has_worker(tictoc_id));
  cl_assert_equal_b(false, app_install_has_worker(music_id));
  cl_assert_equal_b(false, app_install_has_worker(sports_id));
  cl_assert_equal_b(true,  app_install_has_worker(bg_counter_id));
  cl_assert_equal_b(false, app_install_has_worker(menu_layer_id));

  cl_assert_equal_b(false, app_install_has_worker(CRAZY_ID));
}

void test_app_install_manager__is_hidden(void) {
  cl_assert_equal_b(false, app_install_is_hidden(tictoc_id));
  cl_assert_equal_b(false, app_install_is_hidden(music_id));
  cl_assert_equal_b(true,  app_install_is_hidden(sports_id));
  cl_assert_equal_b(false, app_install_is_hidden(bg_counter_id));
  cl_assert_equal_b(false, app_install_is_hidden(menu_layer_id));

  cl_assert_equal_b(false, app_install_is_hidden(CRAZY_ID));
}

void test_app_install_manager__is_from_system(void) {
  cl_assert_equal_b(true, app_install_id_from_system(-1000000));
  cl_assert_equal_b(true, app_install_id_from_system(-1));
  cl_assert_equal_b(false, app_install_id_from_system(0));
  cl_assert_equal_b(false, app_install_id_from_system(1));
  cl_assert_equal_b(false, app_install_id_from_system(1000000));
}

void test_app_install_manager__is_from_app_db(void) {
  cl_assert_equal_b(false, app_install_id_from_app_db(-1000000));
  cl_assert_equal_b(false, app_install_id_from_app_db(-1));
  cl_assert_equal_b(false, app_install_id_from_app_db(0));
  cl_assert_equal_b(true, app_install_id_from_app_db(1));
  cl_assert_equal_b(true, app_install_id_from_app_db(1000000));
}

void test_app_install_manager__get_md(void) {
  const PebbleProcessMd *tictoc_md = app_install_get_md(tictoc_id, false);
  cl_assert(tictoc_md != NULL);
  cl_assert_equal_b(false, tictoc_md->has_worker);
  cl_assert_equal_i(ProcessTypeWatchface, tictoc_md->process_type);
  cl_assert_equal_i(ProcessStorageBuiltin, tictoc_md->process_storage);
  app_install_release_md(tictoc_md);

  const PebbleProcessMd *music_md = app_install_get_md(music_id, false);
  cl_assert(music_md != NULL);
  cl_assert_equal_b(false, music_md->has_worker);
  cl_assert_equal_i(ProcessTypeApp, music_md->process_type);
  cl_assert_equal_i(ProcessStorageBuiltin, music_md->process_storage);
  app_install_release_md(music_md);

  const PebbleProcessMd *sports_md = app_install_get_md(sports_id, false);
  cl_assert(sports_md != NULL);
  cl_assert_equal_b(false, sports_md->has_worker);
  cl_assert_equal_i(ProcessTypeApp, sports_md->process_type);
  cl_assert_equal_i(ProcessStorageBuiltin, sports_md->process_storage);
  app_install_release_md(sports_md);

  const PebbleProcessMd *bg_counter_md = app_install_get_md(bg_counter_id, false);
  cl_assert(bg_counter_md != NULL);
  cl_assert_equal_b(true, bg_counter_md->has_worker);
  cl_assert_equal_i(ProcessTypeApp, bg_counter_md->process_type);
  cl_assert_equal_i(ProcessStorageFlash, bg_counter_md->process_storage);
  app_install_release_md(bg_counter_md);

  const PebbleProcessMd *bg_counter_md_worker = app_install_get_md(bg_counter_id, true);
  cl_assert(bg_counter_md_worker != NULL);
  cl_assert_equal_b(true, bg_counter_md_worker->has_worker);
  cl_assert_equal_i(ProcessTypeWorker, bg_counter_md_worker->process_type);
  cl_assert_equal_i(ProcessStorageFlash, bg_counter_md_worker->process_storage);
  app_install_release_md(bg_counter_md_worker);

  const PebbleProcessMd *menu_layer_md = app_install_get_md(menu_layer_id, false);
  cl_assert(menu_layer_md != NULL);
  cl_assert_equal_b(false, menu_layer_md->has_worker);
  cl_assert_equal_i(ProcessTypeApp, menu_layer_md->process_type);
  cl_assert_equal_i(ProcessStorageFlash, menu_layer_md->process_storage);
  app_install_release_md(menu_layer_md);
}

static bool prv_each(AppInstallEntry *entry, void *data) {
  uint8_t *num_entries = (uint8_t *)data;
  *num_entries = *num_entries + 1;
  cl_assert(entry->install_id != INSTALL_ID_INVALID);
  return true;
}

void test_app_install_manager__enumerate_entries(void) {
  uint8_t num_entries = 0;
  app_install_enumerate_entries(prv_each, (void *)&num_entries);

  // 12 = number of flash apps + system apps
  cl_assert_equal_i(12, num_entries);
}

void test_app_install_manager__hidden_app_recently_communicated(void) {
  static const uint32_t INIT_TIME = 1388563200;
  fake_rtc_init(0, INIT_TIME);

  AppInstallEntry entry;
  cl_assert(true  == app_install_get_entry_for_install_id(sports_id, &entry));
  // hidden before communication
  cl_assert(true == app_install_entry_is_hidden(&entry));

  // simulates multiple messages from app
  for (int i = 0; i < 10; i++) {
    // visible after communication
    app_install_mark_prioritized(sports_id, true /* can_expire */);
    cl_assert(false == app_install_entry_is_hidden(&entry));
  }

  // clear and ensure hidden
  app_install_unmark_prioritized(sports_id);
  cl_assert(true == app_install_entry_is_hidden(&entry));

  // simulates multiple messages from app
  for (int i = 0; i < 10; i++) {
    // visible after communication
    app_install_mark_prioritized(sports_id, true /* can_expire */);
    cl_assert(false == app_install_entry_is_hidden(&entry));
  }

  // wait 10 minutes and ensure hidden
  fake_rtc_init(0, INIT_TIME + (10 * SECONDS_PER_MINUTE));
  cl_assert(true == app_install_entry_is_hidden(&entry));
}

void test_app_install_manager__recently_communicated(void) {
  static const uint32_t INIT_TIME = 1388563200;
  fake_rtc_init(0, INIT_TIME);

  cl_assert_equal_b(false, app_install_is_prioritized(music_id));

  // Update most recent time.
  app_install_mark_prioritized(music_id, true /* can_expire */);
  cl_assert_equal_b(true, app_install_is_prioritized(music_id));

  // Clear recent time.
  app_install_unmark_prioritized(music_id);
  cl_assert_equal_b(false, app_install_is_prioritized(music_id));

  // Update most recent time and let it expire
  app_install_mark_prioritized(music_id, true /* can_expire */);
  cl_assert_equal_b(true, app_install_is_prioritized(music_id));

  // Wait 10 minutes. Should return false
  fake_rtc_increment_time(10 * SECONDS_PER_MINUTE);
  cl_assert_equal_b(false, app_install_is_prioritized(music_id));

  // Update with most recent time but don't let it expire
  app_install_mark_prioritized(music_id, false /* can_expire */);
  fake_rtc_increment_time(10 * SECONDS_PER_MINUTE);

  // Ensure it hasn't expired
  cl_assert_equal_b(true, app_install_is_prioritized(music_id));

  // Manually expire
  app_install_unmark_prioritized(music_id);
  cl_assert_equal_b(false, app_install_is_prioritized(music_id));
}
