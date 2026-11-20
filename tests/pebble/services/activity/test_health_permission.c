/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>
#include <pbl/kernel/malloc.h>
#include <pbl/services/activity/health_permission.h>
#include <pbl/services/blob_db/app_db.h>

#include <clar.h>
#include <fake_mutex.h>
#include <fake_pbl_malloc.h>
#include <fake_settings.h>
#include <process_management/app_install_manager_private.h>
#include <process_management/app_manager.h>
#include <process_management/pebble_process_md.h>
#include <process_management/worker_manager.h>
#include <stubs_logging.h>

static const Uuid s_app = {.byte0 = 1};
static const Uuid s_worker = {.byte0 = 2};
static PebbleProcessMd s_app_md;
static PebbleProcessMd s_worker_md;

const PebbleProcessMd *app_manager_get_current_app_md(void) {
  return &s_app_md;
}

const PebbleProcessMd *worker_manager_get_current_worker_md(void) {
  return &s_worker_md;
}

int pbl_fs_unlink(const char *path) {
  fake_settings_reset();
  return 0;
}

void app_install_clear_app_db(void) {
}
bool app_fetch_in_progress(void) {
  return false;
}
void app_fetch_cancel_from_sys_work(void) {
}
bool app_install_do_callbacks(InstallEventType type, AppInstallId id, Uuid *uuid,
                              InstallCallbackDoneCallback cb, void *data) {
  pbl_free(uuid);
  return true;
}

static void prv_insert(const Uuid *uuid, uint32_t permissions, size_t len) {
  AppDBEntry entry = {.uuid = *uuid, .permissions = permissions};
  cl_assert_equal_i(0, app_db_insert((uint8_t *)uuid, UUID_SIZE, (uint8_t *)&entry, len));
}

void test_health_permission__initialize(void) {
  fake_settings_reset();
  app_db_init();
  s_app_md = (PebbleProcessMd){.uuid = s_app};
  s_worker_md = (PebbleProcessMd){.uuid = s_worker};
  health_permission_handle_changed(nullptr);
}

void test_health_permission__cleanup(void) {
  app_db_flush();
}

void test_health_permission__only_an_enforced_ungranted_app_is_denied(void) {
  prv_insert(&s_app, 0, APP_DB_LEGACY_ENTRY_SIZE);
  cl_assert(health_permission_allowed(PebbleTask_App));
  prv_insert(&s_app, APP_DB_PERMISSION_MICROPHONE, sizeof(AppDBEntry));
  cl_assert(health_permission_allowed(PebbleTask_App));
  prv_insert(&s_app, APP_DB_PERMISSION_HEALTH_ENFORCED, sizeof(AppDBEntry));
  cl_assert(!health_permission_allowed(PebbleTask_App));
  prv_insert(&s_app, APP_DB_PERMISSION_HEALTH_ENFORCED | APP_DB_PERMISSION_HEALTH_GRANTED,
             sizeof(AppDBEntry));
  cl_assert(health_permission_allowed(PebbleTask_App));
}

void test_health_permission__changes_reach_running_processes(void) {
  prv_insert(&s_app, APP_DB_PERMISSION_HEALTH_ENFORCED, sizeof(AppDBEntry));
  prv_insert(&s_worker, APP_DB_PERMISSION_HEALTH_ENFORCED, sizeof(AppDBEntry));
  cl_assert(!health_permission_allowed(PebbleTask_App));
  cl_assert(!health_permission_allowed(PebbleTask_Worker));
  // A legacy resend leaves the permissions alone
  prv_insert(&s_app, 0, APP_DB_LEGACY_ENTRY_SIZE);
  cl_assert(!health_permission_allowed(PebbleTask_App));
  prv_insert(&s_worker, APP_DB_PERMISSION_HEALTH_ENFORCED | APP_DB_PERMISSION_HEALTH_GRANTED,
             sizeof(AppDBEntry));
  cl_assert(!health_permission_allowed(PebbleTask_App));
  cl_assert(health_permission_allowed(PebbleTask_Worker));
  app_db_delete((uint8_t *)&s_app, UUID_SIZE);
  cl_assert(health_permission_allowed(PebbleTask_App));
}

void test_health_permission__a_new_process_gets_its_own_answer(void) {
  prv_insert(&s_app, APP_DB_PERMISSION_HEALTH_ENFORCED, sizeof(AppDBEntry));
  cl_assert(!health_permission_allowed(PebbleTask_App));
  s_app_md.uuid = s_worker;
  cl_assert(health_permission_allowed(PebbleTask_App));
  cl_assert(health_permission_allowed(PebbleTask_KernelMain));
}
