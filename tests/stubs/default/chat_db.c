/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/chat_db.h>

PBL_WEAK void chat_db_init(void) {
  return;
}

PBL_WEAK int chat_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  return 0;
}

PBL_WEAK int chat_db_get_len(const uint8_t *key, int key_len) {
  return 1;
}

PBL_WEAK int chat_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  return 0;
}

PBL_WEAK int chat_db_delete(const uint8_t *key, int key_len) {
  return 0;
}

PBL_WEAK int chat_db_flush(void) {
  return 0;
}

PBL_WEAK int chat_db_compact(void) {
  return 0;
}
