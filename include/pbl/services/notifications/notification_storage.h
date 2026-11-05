/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/uuid.h>

#include <kernel/events.h>

/**
 * @defgroup services_notifications_notification_storage Notification storage
 * @ingroup services_notifications
 * @brief Flash storage for received notifications.
 *
 * Notifications are appended to a single file as a SerializedTimelineItemHeader followed by the
 * serialized attributes and actions. Removing a notification only marks it deleted; deleted
 * entries are dropped when the file is compacted, and the oldest notifications are deleted when
 * the file is full. The file is wiped at boot.
 *
 * Every function takes the storage mutex, which is recursive; use notification_storage_lock() to
 * group several calls.
 * @{
 */

/** @brief Initialize storage, discarding all stored notifications. */
void notification_storage_init(void);

/** @brief Lock the storage mutex (recursive). */
void notification_storage_lock(void);

/** @brief Unlock the storage mutex. */
void notification_storage_unlock(void);

/**
 * @brief Store a notification.
 *
 * Storage is compacted, deleting the oldest notifications if needed, when the file is full. On a
 * write error all notifications are discarded.
 *
 * @param notification Notification to store; it is serialized, the caller keeps ownership.
 */
void notification_storage_store(TimelineItem *notification);

/**
 * @brief Check whether a notification is stored, including one marked deleted.
 *
 * @param id Notification id.
 * @return true if found.
 */
bool notification_storage_notification_exists(const Uuid *id);

/**
 * @brief Get the serialized size of a stored notification.
 *
 * @param uuid Notification id.
 * @return Size of the header and payload in bytes, 0 if not found.
 */
size_t notification_storage_get_len(const Uuid *uuid);

/**
 * @brief Read a notification from storage.
 *
 * @param id Notification id.
 * @param[out] item_out Notification. Its allocated_buffer comes from the calling task's heap and
 *                      must be freed with timeline_item_free_allocated_buffer().
 * @return true on success, false if not found or unreadable.
 */
bool notification_storage_get(const Uuid *id, TimelineItem *item_out);

/**
 * @brief Set the status of a stored notification.
 *
 * @param id Notification id.
 * @param status New status, a combination of TimelineItemStatus flags.
 */
void notification_storage_set_status(const Uuid *id, uint8_t status);

/**
 * @brief Get the status of a stored notification.
 *
 * @param id Notification id.
 * @param[out] status Status, a combination of TimelineItemStatus flags.
 * @return true if found and not marked deleted.
 */
bool notification_storage_get_status(const Uuid *id, uint8_t *status);

/**
 * @brief Remove a notification by marking it deleted.
 *
 * @param id Notification id.
 */
void notification_storage_remove(const Uuid *id);

/**
 * @brief Find the most recent notification with an ANCS UID.
 *
 * iOS can reuse ANCS UIDs after a reconnection, so the newest match wins.
 *
 * @param ancs_uid ANCS UID to look for.
 * @param[out] uuid_out Id of the matching notification.
 * @return true if found.
 */
bool notification_storage_find_ancs_notification_id(uint32_t ancs_uid, Uuid *uuid_out);

/**
 * @brief Find a stored notification identical to a given one.
 *
 * Matches the timestamp, layout and serialized attributes and actions.
 *
 * @param notification Notification to match.
 * @param[out] header_out Header of the matching notification.
 * @return true if a match was found.
 */
bool notification_storage_find_ancs_notification_by_timestamp(TimelineItem *notification,
                                                              CommonTimelineItemHeader *header_out);

/**
 * @brief Iterate over the headers of all notifications not marked deleted.
 *
 * Do not call other notification storage functions from @p iter_callback; doing so corrupts
 * storage.
 *
 * @param iter_callback Called with @p data and each header; return false to stop.
 * @param data Context for @p iter_callback.
 */
void notification_storage_iterate(bool (*iter_callback)(void *data,
                                                        SerializedTimelineItemHeader *header_id),
                                  void *data);

/**
 * @brief Iterate over all notifications, reading selected string attributes of recent ones.
 *
 * For notifications with a timestamp at or after @p item_cutoff, the string attributes listed in
 * @p attr_list are read into their cstring buffers (@p buffer_size bytes each, empty when absent)
 * without deserializing or allocating the payload, and the callback gets an item carrying the
 * header and that list. Older notifications get a NULL item. Corrupt and deleted entries are
 * skipped. Callback arguments are only valid during the callback.
 *
 * Do not call other notification storage functions from @p iter_callback.
 *
 * @param item_cutoff Oldest timestamp for which strings are read.
 * @param attr_list String attributes to read; each cstring points to a buffer of @p buffer_size.
 * @param buffer_size Size of each string buffer in bytes, including the terminator.
 * @param iter_callback Called with @p data, the header and the item or NULL; return false to stop.
 * @param data Context for @p iter_callback.
 */
void notification_storage_iterate_strings_after(
    time_t item_cutoff, AttributeList *attr_list, size_t buffer_size,
    bool (*iter_callback)(void *data, const CommonTimelineItemHeader *header,
                          const TimelineItem *item),
    void *data);

/**
 * @brief Rewrite all notifications through a callback.
 *
 * Each notification not marked deleted is deserialized, passed to @p iter_callback, and written
 * to a new file along with its header, so changes made by the callback are persisted. Deleted
 * entries are dropped.
 *
 * @param iter_callback Called with each notification, its header and @p data.
 * @param data Context for @p iter_callback.
 */
void notification_storage_rewrite(void (*iter_callback)(TimelineItem *notification,
                                                        SerializedTimelineItemHeader *header,
                                                        void *data),
                                  void *data);

/** @brief Discard all notifications and reset the storage state. */
void notification_storage_reset_and_init(void);

#if UNITTEST
/** @brief Discard all notifications and reset the storage state. Unit tests only. */
void notification_storage_reset(void);
#endif

/** @} */
