/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"
#include "services/normal/blob_db/api.h"
#include "util/attributes.h"

status_t WEAK blob_db_delete(BlobDBId db_id, const uint8_t *key, int key_len) {
  return S_SUCCESS;
}


void WEAK blob_db_event_put(enum BlobDBEventType type, BlobDBId db_id, const uint8_t *key,
                            int key_len) {}
