/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_manager.h"
#include "process_management/pebble_process_md.h"
#include "process_management/process_manager.h"
#include "pbl/kernel/compiler.h"

bool PBL_WEAK app_manager_is_initialized(void) {
  return true;
}

static int s_app_manager_launch_new_app__callcount;
static AppLaunchConfig s_app_manager_launch_new_app__config;
bool PBL_WEAK app_manager_launch_new_app(const AppLaunchConfig *config) {
  s_app_manager_launch_new_app__callcount++;
  s_app_manager_launch_new_app__config = *config;
  return true;
}

void PBL_WEAK app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {
  return;
}

ProcessContext *PBL_WEAK app_manager_get_task_context(void) {
  return NULL;
}

void PBL_WEAK app_manager_close_current_app(bool gracefully) {
  return;
}

AppInstallId PBL_WEAK app_manager_get_current_app_id(void) {
  return 0;
}

AppInstallId PBL_WEAK sys_app_manager_get_current_app_id(void) {
  return 0;
}

bool PBL_WEAK app_manager_is_watchface_running(void) {
  return false;
}

WakeupInfo PBL_WEAK app_manager_get_app_wakeup_state(void) {
  return (WakeupInfo){};
}

AppLaunchReason PBL_WEAK app_manager_get_launch_reason(void) {
  return 0;
}

ButtonId PBL_WEAK app_manager_get_launch_button(void) {
  return 0;
}

const PebbleProcessMd *PBL_WEAK app_manager_get_current_app_md(void) {
  return NULL;
}

bool PBL_WEAK app_manager_is_app_supported(const PebbleProcessMd *app_md) {
  return true;
}

Version sys_get_current_app_sdk_version(void) {
  return (Version){PROCESS_INFO_CURRENT_SDK_VERSION_MAJOR, PROCESS_INFO_CURRENT_SDK_VERSION_MINOR};
}

void PBL_WEAK app_manager_get_framebuffer_size(GSize *size) {
}
