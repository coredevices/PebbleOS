/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/pin_db.h"
#include "util/attributes.h"

bool WEAK pin_db_has_entry_expired(time_t pin_end_timestamp) {
  return false;
}

status_t WEAK pin_db_get(const TimelineItemId *id, TimelineItem *pin) {
  return S_SUCCESS;
}

status_t WEAK pin_db_insert_item(TimelineItem *item) {
  return S_SUCCESS;
}

status_t WEAK pin_db_each(TimelineItemStorageEachCallback each, void *data) {
  return S_SUCCESS;
}

status_t WEAK pin_db_delete_with_parent(const TimelineItemId *parent_id) {
  return S_SUCCESS;
}

bool WEAK pin_db_exists_with_parent(const TimelineItemId *parent_id) {
  return true;
}

status_t WEAK pin_db_next_item_header(TimelineItem *next_item_out,
                                 TimelineItemStorageFilterCallback filter) {
  return S_SUCCESS;
}

void WEAK pin_db_init(void) {}

void WEAK pin_db_deinit(void) {}

status_t WEAK pin_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  return S_SUCCESS;
}

int WEAK pin_db_get_len(const uint8_t *key, int key_len) {
  return 1;
}

status_t WEAK pin_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_len) {
  return S_SUCCESS;
}

status_t WEAK pin_db_delete(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t WEAK pin_db_flush(void) {
  return S_SUCCESS;
}

status_t WEAK pin_db_read_item_header(TimelineItem *item_out, TimelineItemId *id) {
  return S_SUCCESS;
}

status_t WEAK pin_db_is_dirty(bool *is_dirty_out) {
  return S_SUCCESS;
}

BlobDBDirtyItem *WEAK pin_db_get_dirty_list(void) {
  return NULL;
}

status_t WEAK pin_db_mark_synced(const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

status_t WEAK pin_db_set_status_bits(const TimelineItemId *id, uint8_t status) {
  return S_SUCCESS;
}
