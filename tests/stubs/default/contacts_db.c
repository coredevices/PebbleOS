/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/contacts_db.h>

PBL_WEAK int contacts_db_get_serialized_contact(const Uuid *uuid, SerializedContact **contact_out) {
  return 0;
}

PBL_WEAK void contacts_db_free_serialized_contact(SerializedContact *contact) {
  return;
}

PBL_WEAK void contacts_db_init(void) {
  return;
}

PBL_WEAK status_t contacts_db_insert(const uint8_t *key, int key_len, const uint8_t *val,
                                     int val_len) {
  return S_SUCCESS;
}

PBL_WEAK int contacts_db_get_len(const uint8_t *key, int key_len) {
  return 1;
}

PBL_WEAK status_t contacts_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t contacts_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

PBL_WEAK status_t contacts_db_flush(void) {
  return S_SUCCESS;
}

PBL_WEAK status_t contacts_db_compact(void) {
  return S_SUCCESS;
}
