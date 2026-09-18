/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/app_permissions/app_permissions_types.h"
#include "pbl/util/attributes.h"
#include "pbl/util/uuid.h"
#include "system/status_codes.h"

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t AppPermissionMask;

#define APP_PERMISSION_BIT(permission) ((AppPermissionMask)1u << (permission))

#define APP_PERMISSIONS_DB_ENTRY_VERSION (1)

//! Serialized grant record pushed by the phone (BlobDBIdAppPermissions, keyed by app Uuid).
typedef struct PACKED AppPermissionsDBEntry {
  uint8_t version; //!< APP_PERMISSIONS_DB_ENTRY_VERSION
  uint8_t reserved[3];
  AppPermissionMask granted_mask;  //!< Permissions the user granted
  AppPermissionMask declared_mask; //!< Permissions the app declared (informational)
} AppPermissionsDBEntry;

_Static_assert(sizeof(AppPermissionsDBEntry) == 12, "AppPermissionsDBEntry layout changed");

//! Per-app permission grants, pushed by the phone and keyed by app Uuid.

//! Reads the grant record for an app.
//! @return S_SUCCESS, E_DOES_NOT_EXIST if there is no record, or another error
status_t app_permissions_db_get(const Uuid *uuid, AppPermissionsDBEntry *entry_out);

//! Writes a grant record for an app. Goes through the generic BlobDB insert path so the same
//! events fire as for a phone-originated insert.
status_t app_permissions_db_set(const Uuid *uuid, const AppPermissionsDBEntry *entry);

//! Deletes the grant record for an app, if any. Goes through the generic BlobDB delete path.
status_t app_permissions_db_delete_for_uuid(const Uuid *uuid);

//! Return false to stop the iteration.
typedef bool (*AppPermissionsDBEachCallback)(const Uuid *uuid, const AppPermissionsDBEntry *entry,
                                             void *context);

//! Iterates over every grant record.
status_t app_permissions_db_each(AppPermissionsDBEachCallback cb, void *context);

///////////////////////////////////////////
// BlobDB Boilerplate (see blob_db/api.h)
///////////////////////////////////////////

void app_permissions_db_init(void);

status_t app_permissions_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                   int val_len);

int app_permissions_db_get_len(const uint8_t *key, int key_len);

status_t app_permissions_db_read(const uint8_t *key, int key_len, uint8_t *val_out,
                                 int val_out_len);

status_t app_permissions_db_delete(const uint8_t *key, int key_len);

status_t app_permissions_db_flush(void);

status_t app_permissions_db_compact(void);
