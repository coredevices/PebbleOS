/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/mutex.h>
#include <pbl/kernel/syscall.h>
#include <pbl/services/activity/health_permission.h>
#include <pbl/services/blob_db/app_db.h>

#include <process_management/app_manager.h>
#include <process_management/pebble_process_md.h>
#include <process_management/worker_manager.h>

typedef struct {
  Uuid uuid;
  bool valid;
  bool allowed;
} HealthPermission;

// Health calls are frequent, so each running process's answer is cached instead of read from flash
static PBL_MUTEX_DEFINE(s_lock);
static HealthPermission s_app;
static HealthPermission s_worker;

bool health_permission_allowed(PebbleTask task) {
  HealthPermission *cached;
  const PebbleProcessMd *md;
  if (task == PebbleTask_App) {
    cached = &s_app;
    md = app_manager_get_current_app_md();
  } else if (task == PebbleTask_Worker) {
    cached = &s_worker;
    md = worker_manager_get_current_worker_md();
  } else {
    return true;
  }
  if (!md) {
    return true;
  }
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!cached->valid || !uuid_equal(&cached->uuid, &md->uuid)) {
    cached->uuid = md->uuid;
    cached->allowed = app_db_health_allowed(&md->uuid);
    cached->valid = true;
  }
  const bool allowed = cached->allowed;
  pbl_mutex_unlock(&s_lock);
  return allowed;
}

void health_permission_handle_changed(const Uuid *uuid) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!uuid || uuid_equal(uuid, &s_app.uuid)) {
    s_app.valid = false;
  }
  if (!uuid || uuid_equal(uuid, &s_worker.uuid)) {
    s_worker.valid = false;
  }
  pbl_mutex_unlock(&s_lock);
}

PBL_SYSCALL(bool, sys_activity_health_allowed) {
  return health_permission_allowed(pebble_task_get_current());
}
