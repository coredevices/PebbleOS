/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/api.h>

#include <kernel/events.h>

int PBL_WEAK blob_db_delete(BlobDBId db_id, const uint8_t *key, int key_len) {
  return 0;
}

void PBL_WEAK blob_db_event_put(enum BlobDBEventType type, BlobDBId db_id, const uint8_t *key,
                                int key_len) {
}
