/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pebble_process_md.h"
#include "process_manager.h"
#include <stdbool.h>

// Worker management functions
void worker_manager_init(void);

bool worker_manager_launch_new_worker_with_args(const PebbleProcessMd *app_md, const void *args);

void worker_manager_launch_next_worker(AppInstallId previous_worker_install_id);

//! Close the worker and never try to open it again until it is asked to.
void worker_manager_handle_remove_current_worker(void);

void worker_manager_close_current_worker(bool gracefully);

const PebbleProcessMd* worker_manager_get_current_worker_md(void);

AppInstallId worker_manager_get_current_worker_id(void);

ProcessContext* worker_manager_get_task_context(void);

//! Exit the worker. Do some cleanup to make sure things close nicely.
//! Called from the worker task
NORETURN worker_task_exit(void);

void worker_manager_put_launch_worker_event(AppInstallId id);

//! Get the AppInstallId of the default worker to launch. Returns INSTALL_ID_INVALID if none set.
AppInstallId worker_manager_get_default_install_id(void);

void worker_manager_set_default_install_id(AppInstallId id);

//! Can be called to enable worker support. Used by the watch-only mode for example
//! to re-enable workers once the battery level recovers
void worker_manager_enable(void);

//! Can be called to disable worker support. Used by the watch-only  mode for example
//! to disable workers when the battery is low. The current worker (if any) will be shut down.
void worker_manager_disable(void);
