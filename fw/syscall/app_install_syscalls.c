/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>
#include <pbl/services/blob_db/app_db.h>
#include <pbl/util/string.h>

#include <applib/app_install.h>
#include <applib/app_launch_reason.h>
#include <process_management/app_install_manager.h>
#include <process_management/app_manager.h>
#include <shell/normal/watchface.h>
#include <syscall/syscall.h>

static_assert(APP_INSTALL_NAME_SIZE == APP_NAME_SIZE_BYTES,
              "installed-app names must fit the registry name");
static_assert(sizeof(AppInstallInfo) <= PBL_SYSCALL_COPY_MAX,
              "AppInstallInfo is too large to copy out of a syscall");

static void prv_copy_name(char *dst, const char *src) {
  size_t len = strnlen(src, APP_NAME_SIZE_BYTES);
  if (len >= APP_INSTALL_NAME_SIZE) {
    len = APP_INSTALL_NAME_SIZE - 1;
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
}

static void prv_fill_info(const AppInstallEntry *entry, AppInstallInfo *info) {
  *info = (AppInstallInfo){
    .uuid = entry->uuid,
    .is_watchface = app_install_entry_is_watchface(entry),
  };
  prv_copy_name(info->name, entry->name);
}

typedef struct {
  uint32_t target;
  uint32_t seen;
  AppInstallInfo *info;
  bool found;
} IndexQuery;

static bool prv_visible(const AppInstallEntry *entry) {
  return !app_install_entry_is_hidden(entry);
}

static bool prv_count_visible(AppInstallEntry *entry, void *data) {
  if (prv_visible(entry)) {
    *(uint32_t *)data += 1;
  }
  return true;
}

static bool prv_copy_at_index(AppInstallEntry *entry, void *data) {
  if (!prv_visible(entry)) {
    return true;
  }

  IndexQuery *query = data;
  if (query->seen == query->target) {
    prv_fill_info(entry, query->info);
    query->found = true;
    return false;
  }
  query->seen++;
  return true;
}

static void prv_launch(AppInstallId id) {
  // APP_LAUNCH_SYSTEM leaves the stored default watchface alone.
  app_manager_put_launch_app_event(&(AppLaunchEventConfig){
    .id = id,
    .common.reason = APP_LAUNCH_SYSTEM,
  });
}

static AppInstallResult prv_check_permission(void) {
  Uuid uuid;
  if (!sys_process_manager_get_current_process_uuid(&uuid) || !app_db_app_install_granted(&uuid)) {
    return AppInstallErrPermissionDenied;
  }
  return AppInstallOk;
}

PBL_SYSCALL(AppInstallResult, sys_app_install_count, (OUT_OPT, uint32_t *, count)) {
  const AppInstallResult permission = prv_check_permission();
  if (permission != AppInstallOk) {
    return permission;
  }
  if (count == nullptr) {
    return AppInstallErrFailed;
  }

  *count = 0;
  app_install_enumerate_entries(prv_count_visible, count);
  return AppInstallOk;
}

PBL_SYSCALL(AppInstallResult, sys_app_install_get_at_index, (VAL, uint32_t, index),
            (OUT, AppInstallInfo *, info)) {
  const AppInstallResult permission = prv_check_permission();
  if (permission != AppInstallOk) {
    return permission;
  }
  if (info == nullptr) {
    return AppInstallErrFailed;
  }

  IndexQuery query = {
    .target = index,
    .info = info,
  };
  app_install_enumerate_entries(prv_copy_at_index, &query);
  return query.found ? AppInstallOk : AppInstallErrFailed;
}

PBL_SYSCALL(AppInstallResult, sys_app_launch, (IN, const Uuid *, uuid)) {
  const AppInstallResult permission = prv_check_permission();
  if (permission != AppInstallOk) {
    return permission;
  }
  if (uuid == nullptr) {
    return AppInstallErrFailed;
  }

  const AppInstallId id = app_install_get_id_for_uuid(uuid);
  if (id == INSTALL_ID_INVALID) {
    return AppInstallErrFailed;
  }

  PBL_LOG_DBG("Launching app id %" PRId32, id);
  prv_launch(id);
  return AppInstallOk;
}

PBL_SYSCALL(AppInstallResult, sys_watchface_select, (IN, const Uuid *, uuid)) {
  const AppInstallResult permission = prv_check_permission();
  if (permission != AppInstallOk) {
    return permission;
  }
  if (uuid == nullptr) {
    return AppInstallErrFailed;
  }

  const AppInstallId id = app_install_get_id_for_uuid(uuid);
  if (id == INSTALL_ID_INVALID) {
    return AppInstallErrFailed;
  }

  AppInstallEntry entry;
  if (!app_install_get_entry_for_install_id(id, &entry) ||
      !app_install_entry_is_watchface(&entry) || app_install_entry_is_hidden(&entry)) {
    PBL_LOG_DBG("Refusing watchface select for id %" PRId32, id);
    return AppInstallErrFailed;
  }

  watchface_set_default_install_id(id);
  PBL_LOG_DBG("Selected watchface id %" PRId32, id);
  prv_launch(id);
  return AppInstallOk;
}
