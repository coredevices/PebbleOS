/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/util/uuid.h>

#include <process_management/app_install_manager.h>
#include <process_management/app_install_types.h>

AppInstallId app_install_get_id_for_uuid(const Uuid *uuid);

AppInstallId app_get_install_id_for_uuid_from_registry(const Uuid *uuid);

bool app_install_get_entry_for_install_id(AppInstallId install_id, AppInstallEntry *entry);

bool app_install_get_uuid_for_install_id(AppInstallId install_id, Uuid *uuid_out);

bool app_install_is_watchface(AppInstallId app_id);

bool app_install_id_from_system(AppInstallId id);

bool app_install_id_from_app_db(AppInstallId id);

void app_install_enumerate_entries(AppInstallEnumerateCb cb, void *data);

bool app_install_entry_is_watchface(const AppInstallEntry *entry);

bool app_install_entry_is_hidden(const AppInstallEntry *entry);

bool app_install_entry_is_SDK_compatible(const AppInstallEntry *entry);

const PebbleProcessMd *app_install_get_md(AppInstallId id, bool worker);

ResAppNum app_install_get_app_icon_bank(const AppInstallEntry *entry);
