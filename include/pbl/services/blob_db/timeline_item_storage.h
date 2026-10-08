/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/uuid.h>
#include <pbl/services/regular_timer.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/timeline/item.h>

/**
 * @defgroup services_blob_db_timeline_item_storage Timeline item storage
 * @ingroup services_blob_db
 * @brief Settings file store of timeline items, shared by the pin and reminder databases.
 *
 * Items are keyed by UUID. The @c flags and @c status bytes of the item header are stored
 * inverted; the read API restores them.
 * @{
 */

/** @brief Timeline item store backed by a settings file. */
typedef struct {
  /** Backing settings file, kept open between init and deinit. */
  SettingsFile file;
  /** Serializes access to @ref file. */
  struct pbl_mutex mutex;
  /** Settings file name. */
  char *name;
  /** Maximum settings file size in bytes. */
  size_t max_size;
  /** Age in seconds past which items are rejected or skipped. */
  uint32_t max_item_age;
} TimelineItemStorage;

/**
 * @brief Filter for timeline_item_storage_next_item().
 *
 * @param hdr Item header, with flags and status restored.
 * @param context Iteration context of the caller, not user data.
 * @return true to consider the item, false to skip it.
 */
typedef bool (*TimelineItemStorageFilterCallback)(SerializedTimelineItemHeader *hdr, void *context);

/**
 * @brief Callback for timeline_item_storage_each().
 *
 * @warning @c flags and @c status of the stored CommonTimelineItemHeader are inverted and are not
 * restored for the callback.
 */
typedef SettingsFileEachCallback TimelineItemStorageEachCallback;

/**
 * @brief Callback of timeline_item_storage_delete_with_parent().
 *
 * @param id UUID of the deleted child.
 */
typedef void (*TimelineItemStorageChildDeleteCallback)(const Uuid *id);

/**
 * @brief Initialize a storage and open its settings file.
 *
 * @param[out] storage Storage.
 * @param filename Settings file name; must outlive the storage.
 * @param max_size Maximum file size in bytes.
 * @param max_age Age in seconds past which items are rejected or skipped.
 */
void timeline_item_storage_init(TimelineItemStorage *storage, char *filename, uint32_t max_size,
                                uint32_t max_age);

/**
 * @brief Close the settings file of a storage.
 *
 * @param storage Storage.
 */
void timeline_item_storage_deinit(TimelineItemStorage *storage);

/**
 * @brief Compact and shrink the backing settings file.
 *
 * @param storage Storage.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t timeline_item_storage_compact(TimelineItemStorage *storage);

/**
 * @brief Check whether any item has a given parent.
 *
 * @param storage Storage.
 * @param parent_id Parent UUID.
 * @return true if at least one item has this parent.
 */
bool timeline_item_storage_exists_with_parent(TimelineItemStorage *storage, const Uuid *parent_id);

/**
 * @brief Delete all items except those created on the watch.
 *
 * @param storage Storage.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t timeline_item_storage_flush(TimelineItemStorage *storage);

/**
 * @brief Delete an item.
 *
 * @param storage Storage.
 * @param key Item UUID.
 * @param key_len Length of @p key, must be @c UUID_SIZE.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t timeline_item_storage_delete(TimelineItemStorage *storage, const uint8_t *key,
                                      int key_len);

/**
 * @brief Read a serialized item.
 *
 * Flags and status of the header are restored.
 *
 * @param storage Storage.
 * @param key Item UUID.
 * @param key_len Length of @p key, must be @c UUID_SIZE.
 * @param[out] val_out Buffer for the serialized item.
 * @param val_len Size of @p val_out in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t timeline_item_storage_read(TimelineItemStorage *storage, const uint8_t *key, int key_len,
                                    uint8_t *val_out, int val_len);

/**
 * @brief Deserialize the item of a settings file record, from an each callback.
 *
 * Temporarily allocates the whole record on the kernel heap; use sparingly.
 *
 * @param file Settings file being iterated.
 * @param info Current record.
 * @param[out] item Item; free its buffer with timeline_item_free_allocated_buffer().
 * @return @c S_SUCCESS on success, @c E_INTERNAL if the record cannot be deserialized.
 */
status_t timeline_item_storage_get_from_settings_record(SettingsFile *file,
                                                        SettingsRecordInfo *info,
                                                        TimelineItem *item);

/**
 * @brief Overwrite the status bits of an item in place.
 *
 * @param storage Storage.
 * @param key Item UUID.
 * @param key_len Length of @p key, must be @c UUID_SIZE.
 * @param status New status bits.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t timeline_item_storage_set_status_bits(TimelineItemStorage *storage, const uint8_t *key,
                                               int key_len, uint8_t status);

/**
 * @brief Get the length of a serialized item.
 *
 * @param storage Storage.
 * @param key Item UUID.
 * @param key_len Length of @p key in bytes.
 * @return Length in bytes, 0 if not found, or a negative error code.
 */
int timeline_item_storage_get_len(TimelineItemStorage *storage, const uint8_t *key, int key_len);

/**
 * @brief Insert or replace a serialized item.
 *
 * The item layout is validated, and items whose end time is older than the maximum age are
 * rejected.
 *
 * @param storage Storage.
 * @param key Item UUID.
 * @param key_len Length of @p key, must be @c UUID_SIZE.
 * @param val Serialized item. Modified during the call and restored before returning.
 * @param val_len Length of @p val in bytes.
 * @param mark_as_synced Store the record as synced, i.e. not to be written back to the phone.
 * @retval S_SUCCESS Inserted.
 * @retval E_INVALID_ARGUMENT Malformed key or item.
 * @retval E_INVALID_OPERATION Item too old.
 * @return Other error codes on failure.
 */
status_t timeline_item_storage_insert(TimelineItemStorage *storage, const uint8_t *key, int key_len,
                                      const uint8_t *val, int val_len, bool mark_as_synced);

/**
 * @brief Call a function for every record, with the storage locked.
 *
 * @warning @c flags and @c status of the stored CommonTimelineItemHeader are inverted and are not
 * restored for the callback.
 *
 * @param storage Storage.
 * @param each Callback.
 * @param data User data passed to @p each.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t timeline_item_storage_each(TimelineItemStorage *storage,
                                    TimelineItemStorageEachCallback each, void *data);

/**
 * @brief Mark an item as synced.
 *
 * @param storage Storage.
 * @param key Item UUID.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t timeline_item_storage_mark_synced(TimelineItemStorage *storage, const uint8_t *key,
                                           int key_len);

/**
 * @brief Delete the children of a parent.
 *
 * At most three children are deleted per call.
 *
 * @param storage Storage.
 * @param parent_id Parent UUID.
 * @param child_delete_cb Optional callback invoked for each deleted child.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
status_t timeline_item_storage_delete_with_parent(
    TimelineItemStorage *storage, const Uuid *parent_id,
    TimelineItemStorageChildDeleteCallback child_delete_cb);

/**
 * @brief Find the earliest item that is not older than the maximum age.
 *
 * @param storage Storage.
 * @param[out] id_out UUID of the item.
 * @param filter_cb Optional filter.
 * @retval S_SUCCESS Found.
 * @retval S_NO_MORE_ITEMS No matching item.
 * @return Other error codes on failure.
 */
status_t timeline_item_storage_next_item(TimelineItemStorage *storage, Uuid *id_out,
                                         TimelineItemStorageFilterCallback filter_cb);

/**
 * @brief Check whether the storage holds no valid item.
 *
 * @param storage Storage.
 * @return true if empty.
 */
bool timeline_item_storage_is_empty(TimelineItemStorage *storage);

/** @} */
