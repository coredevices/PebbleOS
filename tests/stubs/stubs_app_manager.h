/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <process_management/app_manager.h>
#include <process_management/pebble_process_md.h>
#include <process_management/process_manager.h>

extern int s_app_manager_launch_new_app__callcount;
extern AppLaunchConfig s_app_manager_launch_new_app__config;

bool app_manager_is_initialized(void);

bool app_manager_launch_new_app(const AppLaunchConfig *config);

void app_manager_put_launch_app_event(const AppLaunchEventConfig *config);

ProcessContext *app_manager_get_task_context(void);

void app_manager_close_current_app(bool gracefully);

AppInstallId app_manager_get_current_app_id(void);

AppInstallId sys_app_manager_get_current_app_id(void);

bool app_manager_is_watchface_running(void);

WakeupInfo app_manager_get_app_wakeup_state(void);

AppLaunchReason app_manager_get_launch_reason(void);

ButtonId app_manager_get_launch_button(void);

const PebbleProcessMd *app_manager_get_current_app_md(void);

bool app_manager_is_app_supported(const PebbleProcessMd *app_md);

Version sys_get_current_app_sdk_version(void);

void app_manager_get_framebuffer_size(GSize *size);
