/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/app_permissions/app_permissions_types.h"
#include "pbl/services/blob_db/app_permissions_db.h"
#include "pbl/util/uuid.h"
#include "system/status_codes.h"

#include <stdbool.h>

//! Kernel-side view of per-app permission grants.
//!
//! The phone is the source of truth and pushes grant records into BlobDBIdAppPermissions. This
//! service turns those records plus the app's manifest declaration into an AppPermissionState,
//! and notifies the running app (PEBBLE_APP_PERMISSION_EVENT) when its grants change.
//!
//! Rules:
//! - Not declared in the app's manifest -> NotDeclared, whatever the record says.
//! - System apps -> Granted.
//! - Declared but no record (e.g. old phone app) -> Denied. SDK shell builds default to Granted
//!   so sideloaded apps work without the phone.

//! Subscribes to BlobDB events. Call once at boot after blob_db_init_dbs().
void app_permissions_init(void);

//! @param uuid The app's UUID
//! @param declared Whether the app declares the permission in its manifest
//! @param permission The permission to look up
AppPermissionState app_permissions_get_state_for_app(const Uuid *uuid, bool declared,
                                                     AppPermission permission);

//! State of a permission for the app currently running in the app task (NotDeclared if none).
AppPermissionState app_permissions_get_state_for_current_app(AppPermission permission);

bool app_permissions_is_granted_for_current_app(AppPermission permission);

//! Grants or revokes a permission locally (console / test tooling). Writes through the BlobDB so
//! the same events fire as for a phone-originated change.
status_t app_permissions_set_granted(const Uuid *uuid, AppPermission permission, bool granted);
