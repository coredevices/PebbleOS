/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/app_glance_db.h>

PBL_WEAK void app_glance_db_init(void) {
}

PBL_WEAK status_t app_glance_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                       int val_len) {
  return S_SUCCESS;
}

PBL_WEAK int app_glance_db_get_len(const uint8_t *key, int key_len) {
  return 0;
}

PBL_WEAK status_t app_glance_db_read(const uint8_t *key, int key_len, uint8_t *val_out,
                                     int val_out_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t app_glance_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t app_glance_db_flush(void) {
  return S_SUCCESS;
}

PBL_WEAK status_t app_glance_db_compact(void) {
  return S_SUCCESS;
}
