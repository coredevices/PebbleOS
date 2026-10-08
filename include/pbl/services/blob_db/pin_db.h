/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"
#include "timeline_item_storage.h"

#include <system/status_codes.h>
#include <pbl/services/timeline/item.h>
#include <pbl/util/iterator.h>

#include <stdint.h>

/**
 * @defgroup services_blob_db_pin_db Pin database
 * @ingroup services_blob_db
 * @brief Timeline pins (::BlobDBIdPins), keyed by pin UUID.
 *
 * Backed by @ref services_blob_db_timeline_item_storage; pins older than three days are rejected.
 * @{
 */

/**
 * @brief Read and deserialize a pin.
 *
 * @param id Pin UUID.
 * @param[out] pin Pin; free its buffer with timeline_item_free_allocated_buffer().
 * @retval S_SUCCESS Read.
 * @retval E_DOES_NOT_EXIST No such pin.
 * @return Other error codes on failure.
 */
status_t pin_db_get(const TimelineItemId *id, TimelineItem *pin);

/**
 * @brief Serialize and insert a pin, emitting a BlobDB insert event.
 *
 * Pins from the reminders app data source are marked dirty and synced to the phone.
 *
 * @param item Pin, of type @c TimelineItemTypePin.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_insert_item(TimelineItem *item);

/**
 * @brief pin_db_insert_item() without the BlobDB event.
 *
 * Meant for tests inserting many generated pins, which would otherwise flood the event queue.
 *
 * @param item Pin, of type @c TimelineItemTypePin.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_insert_item_without_event(TimelineItem *item);

/**
 * @brief Overwrite the status bits of a pin.
 *
 * @param id Pin UUID.
 * @param status New status bits.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_set_status_bits(const TimelineItemId *id, uint8_t status);

/**
 * @brief Call a function for every record in the pin database.
 *
 * @warning @c flags and @c status of the stored CommonTimelineItemHeader are inverted and are not
 * restored for the callback.
 *
 * @param each Callback.
 * @param data User data passed to @p each.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_each(TimelineItemStorageEachCallback each, void *data);

/**
 * @brief Delete the pins of a parent (app or data source).
 *
 * @param parent_id Parent UUID.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_delete_with_parent(const TimelineItemId *parent_id);

/**
 * @brief Check whether a parent has pins.
 *
 * @param parent_id Parent UUID.
 * @return true if at least one pin has this parent.
 */
bool pin_db_exists_with_parent(const TimelineItemId *parent_id);

/**
 * @brief Read the header of a pin.
 *
 * @param[out] item_out Item with only the header filled in.
 * @param id Pin UUID.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_read_item_header(TimelineItem *item_out, TimelineItemId *id);

/**
 * @brief Read the header of the earliest pin within the maximum age.
 *
 * @param[out] next_item_out Item with only the header filled in.
 * @param filter Optional filter, returns false to skip an item.
 * @retval S_SUCCESS Found.
 * @retval S_NO_MORE_ITEMS No matching pin.
 * @return Other error codes on failure.
 */
status_t pin_db_next_item_header(TimelineItem *next_item_out,
                                 TimelineItemStorageFilterCallback filter);

/**
 * @brief Check whether a pin is older than the pin database keeps.
 *
 * @param pin_end_timestamp End time of the pin.
 * @return true if the pin has expired.
 */
bool pin_db_has_entry_expired(time_t pin_end_timestamp);

/** @brief Initialize the pin database. */
void pin_db_init(void);

/** @brief Close the settings file backing the pin database. */
void pin_db_deinit(void);

/**
 * @brief Insert or replace a record in the pin database.
 *
 * The key is the pin UUID and the value a serialized timeline item. Records from the phone are
 * stored as synced. Notification and reminder layouts are rejected. Triggers a fetch of the
 * parent app when it is installed but not cached.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the pin database.
 *
 * The key is the pin UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int pin_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the pin database.
 *
 * The key is the pin UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the pin database.
 *
 * The key is the pin UUID. Also deletes the reminders of the pin.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the pin database.
 *
 * Pins created on the watch are kept.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_flush(void);

/**
 * @brief Compact the settings file backing the pin database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_compact(void);

/**
 * @brief Check whether the pin database holds records not yet synced to the phone.
 *
 * @param[out] is_dirty_out Set to true if at least one record is dirty.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_is_dirty(bool *is_dirty_out);

/**
 * @brief Build the list of records of the pin database not yet synced to the phone.
 *
 * @return Heap-allocated list (free with blob_db_util_free_dirty_list()), NULL if none.
 */
BlobDBDirtyItem *pin_db_get_dirty_list(void);

/**
 * @brief Mark a record of the pin database as synced.
 *
 * The key is the pin UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t pin_db_mark_synced(const uint8_t *key, int key_len);

/** @} */
