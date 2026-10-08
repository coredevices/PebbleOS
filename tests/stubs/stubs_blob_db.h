/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/services/blob_db/api.h>

#include <kernel/events.h>

status_t PBL_WEAK blob_db_delete(BlobDBId db_id, const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

void PBL_WEAK blob_db_event_put(enum BlobDBEventType type, BlobDBId db_id, const uint8_t *key,
                                int key_len) {
}
