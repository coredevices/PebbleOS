/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/blob_db/app_permissions_db.h"

status_t app_permissions_db_get(const Uuid *uuid, AppPermissionsDBEntry *entry_out) {
  return E_DOES_NOT_EXIST;
}

status_t app_permissions_db_set(const Uuid *uuid, const AppPermissionsDBEntry *entry) {
  return S_SUCCESS;
}

status_t app_permissions_db_delete_for_uuid(const Uuid *uuid) {
  return S_SUCCESS;
}

status_t app_permissions_db_each(AppPermissionsDBEachCallback cb, void *context) {
  return S_SUCCESS;
}

///////////////////////////////////////////
// BlobDB Boilerplate (see blob_db/api.h)
///////////////////////////////////////////

void app_permissions_db_init(void) {
}

status_t app_permissions_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                   int val_len) {
  return S_SUCCESS;
}

int app_permissions_db_get_len(const uint8_t *key, int key_len) {
  return 0;
}

status_t app_permissions_db_read(const uint8_t *key, int key_len, uint8_t *val_out,
                                 int val_out_len) {
  return S_SUCCESS;
}

status_t app_permissions_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t app_permissions_db_flush(void) {
  return S_SUCCESS;
}

status_t app_permissions_db_compact(void) {
  return S_SUCCESS;
}
