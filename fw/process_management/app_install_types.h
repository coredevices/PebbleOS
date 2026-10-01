/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

//! ID unique to a given app for the duration that it is installed
//! System apps (system/resource) and banked applications are negative numbers.
//! AppDB flash apps are positive numbers
typedef int32_t AppInstallId;

#define INSTALL_ID_INVALID ((AppInstallId)0)

//! Returns true for system applications
bool app_install_id_from_system(AppInstallId id);

//! Returns true for user installed applications
bool app_install_id_from_app_db(AppInstallId id);
