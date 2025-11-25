/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"
#include "timeline_item_storage.h"

#include "system/status_codes.h"
#include "services/normal/timeline/item.h"
#include "util/iterator.h"

#include <stdint.h>

status_t pin_db_get(const TimelineItemId *id, TimelineItem *pin);

status_t pin_db_insert_item(TimelineItem *item);

//! Inserts an item without emitting a BlobDB event.
//! @note This is provided for testing automatically generated pins which would otherwise flood
//! the event queue. Please use \ref pin_db_insert_item instead when possible.
status_t pin_db_insert_item_without_event(TimelineItem *item);

status_t pin_db_set_status_bits(const TimelineItemId *id, uint8_t status);

//! Caution: CommonTimelineItemHeader .flags & .status are stored inverted and not auto-restored
status_t pin_db_each(TimelineItemStorageEachCallback each, void *data);

status_t pin_db_delete_with_parent(const TimelineItemId *parent_id);

bool pin_db_exists_with_parent(const TimelineItemId *parent_id);

status_t pin_db_read_item_header(TimelineItem *item_out, TimelineItemId *id);

status_t pin_db_next_item_header(TimelineItem *next_item_out,
                                 TimelineItemStorageFilterCallback filter);

//! Determines whether or not the timeline entry has expired based on its age
//! @param pin_timestamp - the timestamp of the pin being removed
bool pin_db_has_entry_expired(time_t pin_end_timestamp);


///////////////////////////////////////////
// BlobDB Boilerplate (see blob_db/api.h)
///////////////////////////////////////////

void pin_db_init(void);

void pin_db_deinit(void);

status_t pin_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

int pin_db_get_len(const uint8_t *key, int key_len);

status_t pin_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

status_t pin_db_delete(const uint8_t *key, int key_len);

status_t pin_db_flush(void);

status_t pin_db_is_dirty(bool *is_dirty_out);

BlobDBDirtyItem* pin_db_get_dirty_list(void);

status_t pin_db_mark_synced(const uint8_t *key, int key_len);
