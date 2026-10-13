/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <process_management/app_install_types.h>
#include <system/status_codes.h>

PBL_WEAK AppInstallId app_cache_get_next_eviction(void) {
  return 0;
}

PBL_WEAK void app_cache_init(void) {
  return;
}

PBL_WEAK status_t app_cache_add_entry(AppInstallId app_id) {
  return 0;
}

PBL_WEAK status_t app_cache_entry_exists(AppInstallId app_id) {
  return true;
}

PBL_WEAK status_t app_cache_app_launched(AppInstallId app_id) {
  return 0;
}

PBL_WEAK status_t app_cache_remove_entry(AppInstallId app_id) {
  return 0;
}

PBL_WEAK void app_cache_flush(void) {
  return;
}
