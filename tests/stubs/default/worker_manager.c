/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <process_management/pebble_process_md.h>
#include <process_management/process_manager.h>

// Worker management functions
PBL_WEAK void worker_manager_init(void) {
}

PBL_WEAK bool worker_manager_launch_new_worker_with_args(const PebbleProcessMd *app_md,
                                                         const void *args) {
  return false;
}

PBL_WEAK void worker_manager_close_current_worker(bool gracefully) {
}

PBL_WEAK const PebbleProcessMd *worker_manager_get_current_worker_md(void) {
  return nullptr;
}

static ProcessContext s_worker_task_context;
PBL_WEAK ProcessContext *worker_manager_get_task_context(void) {
  return &s_worker_task_context;
}

PBL_WEAK void worker_manager_put_launch_worker_event(AppInstallId id) {
}

//! Get the AppInstallId of the default worker to launch. Returns INSTALL_ID_INVALID if none set.
PBL_WEAK AppInstallId worker_manager_get_default_install_id(void) {
  return 0;
}

PBL_WEAK AppInstallId worker_manager_get_current_worker_id(void) {
  return 0;
}

PBL_WEAK AppInstallId sys_worker_manager_get_current_worker_id(void) {
  return 0;
}

PBL_WEAK void worker_manager_handle_remove_current_worker(void) {
}
