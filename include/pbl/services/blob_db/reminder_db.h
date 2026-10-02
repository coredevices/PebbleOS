/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api.h"
#include "timeline_item_storage.h"

#include "system/status_codes.h"
#include "pbl/services/timeline/item.h"

/**
 * @defgroup services_blob_db_reminder_db Reminder database
 * @ingroup services_blob_db
 * @brief Timeline reminders (::BlobDBIdReminders), keyed by reminder UUID.
 *
 * Backed by @ref services_blob_db_timeline_item_storage. Reminders more than 15 minutes in the
 * past are rejected.
 * @{
 */

/**
 * @brief Read and deserialize a reminder.
 *
 * @param[out] item_out Reminder; free its buffer with timeline_item_free_allocated_buffer().
 * @param id Reminder UUID.
 * @retval S_SUCCESS Read.
 * @retval E_DOES_NOT_EXIST No such reminder.
 * @return Other error codes on failure.
 */
status_t reminder_db_read_item(TimelineItem *item_out, TimelineItemId *id);

/**
 * @brief Read the header of the earliest reminder that has not fired yet.
 *
 * @param[out] next_item_out Item with only the header filled in.
 * @retval S_SUCCESS Found.
 * @retval S_NO_MORE_ITEMS No pending reminder.
 * @return Other error codes on failure.
 */
status_t reminder_db_next_item_header(TimelineItem *next_item_out);

/**
 * @brief Serialize and insert a reminder created on the watch.
 *
 * The record is marked dirty and synced to the phone.
 *
 * @param item Reminder, of type @c TimelineItemTypeReminder.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_insert_item(TimelineItem *item);

/**
 * @brief Delete a reminder.
 *
 * @param id Reminder UUID.
 * @param send_event If true, also notify the reminders service as reminder_db_delete() does.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_delete_item(const TimelineItemId *id, bool send_event);

/**
 * @brief Delete the reminders of a parent pin.
 *
 * @param parent_id Parent pin UUID.
 * @return @c S_SUCCESS if all reminders were deleted, an error code otherwise.
 */
status_t reminder_db_delete_with_parent(const TimelineItemId *parent_id);

/**
 * @brief Check whether the reminder database is empty.
 *
 * @return true if there are no reminders.
 */
bool reminder_db_is_empty(void);

/**
 * @brief Overwrite the status bits of a reminder.
 *
 * @param id Reminder UUID.
 * @param status New status bits.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_set_status_bits(const TimelineItemId *id, uint8_t status);

/**
 * @brief Find a reminder with a given time and title.
 *
 * @param timestamp Time to match.
 * @param title Title to match.
 * @param filter Optional additional filter, returns false to reject a candidate.
 * @param[out] reminder_out Matching reminder, valid only when true is returned; free its buffer
 * with timeline_item_free_allocated_buffer(). Must not be NULL.
 * @return true if a matching reminder was found.
 */
bool reminder_db_find_by_timestamp_title(time_t timestamp, const char *title,
                                         TimelineItemStorageFilterCallback filter,
                                         TimelineItem *reminder_out);

/**
 * @brief Initialize the reminder database.
 *
 * Also initializes the reminders service.
 */
void reminder_db_init(void);

/** @brief Close the settings file backing the reminder database. */
void reminder_db_deinit(void);

/**
 * @brief Insert or replace a record in the reminder database.
 *
 * The key is the reminder UUID and the value a serialized timeline item. Records from the
 * phone are stored as synced. Updates the reminder timer.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len);

/**
 * @brief Get the length of a record in the reminder database.
 *
 * The key is the reminder UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if not found, or a negative error code.
 */
int reminder_db_get_len(const uint8_t *key, int key_len);

/**
 * @brief Read a record from the reminder database.
 *
 * The key is the reminder UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer for the value.
 * @param val_out_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len);

/**
 * @brief Delete a record from the reminder database.
 *
 * The key is the reminder UUID. Notifies the reminders service.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_delete(const uint8_t *key, int key_len);

/**
 * @brief Delete all records of the reminder database.
 *
 * Reminders created on the watch are kept.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_flush(void);

/**
 * @brief Compact the settings file backing the reminder database.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_compact(void);

/**
 * @brief Check whether the reminder database holds records not yet synced to the phone.
 *
 * @param[out] is_dirty_out Set to true if at least one record is dirty.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_is_dirty(bool *is_dirty_out);

/**
 * @brief Build the list of records of the reminder database not yet synced to the phone.
 *
 * @return Heap-allocated list (free with blob_db_util_free_dirty_list()), NULL if none.
 */
BlobDBDirtyItem *reminder_db_get_dirty_list(void);

/**
 * @brief Mark a record of the reminder database as synced.
 *
 * The key is the reminder UUID.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t reminder_db_mark_synced(const uint8_t *key, int key_len);

/** @} */
