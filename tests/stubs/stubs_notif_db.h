/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

void notif_db_init(void) {
  return;
}

status_t notif_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  return S_SUCCESS;
}

int notif_db_get_len(const uint8_t *key, int key_len) {
  return 1;
}

status_t notif_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  return S_SUCCESS;
}

status_t notif_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t notif_db_flush(void) {
  return S_SUCCESS;
}
