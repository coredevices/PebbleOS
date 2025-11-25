/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/contacts_db.h"

int contacts_db_get_serialized_contact(const Uuid *uuid, SerializedContact **contact_out) {
  return 0;
}

void contacts_db_free_serialized_contact(SerializedContact *contact) {
  return;
}

void contacts_db_init(void) {
  return;
}

status_t contacts_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  return S_SUCCESS;
}

int contacts_db_get_len(const uint8_t *key, int key_len) {
  return 1;
}

status_t contacts_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  return S_SUCCESS;
}

status_t contacts_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t contacts_db_flush(void) {
  return S_SUCCESS;
}
