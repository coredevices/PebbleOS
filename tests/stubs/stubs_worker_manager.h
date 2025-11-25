/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"
#include "process_management/process_manager.h"

// Worker management functions
void worker_manager_init(void) {
}

bool worker_manager_launch_new_worker_with_args(const PebbleProcessMd *app_md, const void *args) {
  return false;
}

void worker_manager_close_current_worker(bool gracefully) {
}

const PebbleProcessMd* worker_manager_get_current_worker_md(void) {
  return NULL;
}

static ProcessContext s_worker_task_context;
ProcessContext* worker_manager_get_task_context(void) {
  return &s_worker_task_context;
}


void worker_manager_put_launch_worker_event(AppInstallId id) {
}

//! Get the AppInstallId of the default worker to launch. Returns INSTALL_ID_INVALID if none set. 
AppInstallId worker_manager_get_default_install_id(void) {
  return 0;
}

AppInstallId worker_manager_get_current_worker_id(void) {
  return 0;
}

AppInstallId sys_worker_manager_get_current_worker_id(void) {
  return 0;
}

void worker_manager_handle_remove_current_worker(void) {
}
