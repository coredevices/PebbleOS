/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <process_management/app_install_types.h>
#include <system/status_codes.h>

AppInstallId app_cache_get_next_eviction(void);

void app_cache_init(void);

status_t app_cache_add_entry(AppInstallId app_id);

status_t app_cache_entry_exists(AppInstallId app_id);

status_t app_cache_app_launched(AppInstallId app_id);

status_t app_cache_remove_entry(AppInstallId app_id);

void app_cache_flush(void);
