/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "api_types.h"

#include <stdint.h>
#include <stdbool.h>

#include "system/status_codes.h"
#include "pbl/kernel/compiler.h"
#include "pbl/util/list.h"
#include <time.h>

/**
 * @defgroup services_blob_db BlobDB
 * @ingroup services
 * @brief Key/value databases the phone synchronizes with the watch.
 *
 * BlobDB is a single API in front of several key/value stores (pins, apps, reminders,
 * notifications, weather, settings, ...), each identified by a ::BlobDBId. The phone writes
 * to them over the Pebble Protocol BlobDB endpoints; the wire format, command opcodes and
 * response codes are described in @c docs/reference/blob-db.md.
 *
 * Each database implements the @c BlobDB*Impl callbacks and is registered with its id in
 * the @c s_blob_dbs table of @c fw/services/blob_db/api.c. Callbacks are blocking and return
 * once the command has been executed; a database is not guaranteed to persist across
 * reboots. Unimplemented operations return @c E_INVALID_OPERATION.
 *
 * Databases that track dirty records (records written on the watch and not yet acknowledged
 * by the phone) can be written back to the phone with the sync API (see
 * @ref services_blob_db_sync).
 *
 * @code{.c}
 * const Uuid key = ...;
 * status_t rv = blob_db_insert(BlobDBIdContacts, (const uint8_t *)&key, sizeof(key), val,
 *                              val_len);
 *
 * int len = blob_db_get_len(BlobDBIdContacts, (const uint8_t *)&key, sizeof(key));
 * if (len > 0) {
 *   uint8_t *buf = kernel_malloc_check(len);
 *   rv = blob_db_read(BlobDBIdContacts, (const uint8_t *)&key, sizeof(key), buf, len);
 *   ...
 *   kernel_free(buf);
 * }
 * @endcode
 * @{
 */

/** @brief Database identifiers, as sent on the wire. */
typedef enum PBL_PACKED {
  /** Test database, not registered. */
  BlobDBIdTest = 0x00,
  /** Timeline pins (see @ref services_blob_db_pin_db). */
  BlobDBIdPins = 0x01,
  /** Installed apps (see @ref services_blob_db_app_db). */
  BlobDBIdApps = 0x02,
  /** Reminders (see @ref services_blob_db_reminder_db). */
  BlobDBIdReminders = 0x03,
  /** Notifications (see @ref services_blob_db_notif_db). */
  BlobDBIdNotifs = 0x04,
  /** Weather locations (see @ref services_blob_db_weather_db). */
  BlobDBIdWeather = 0x05,
  /** iOS notification preferences (see @ref services_blob_db_ios_notif_pref_db). */
  BlobDBIdiOSNotifPref = 0x06,
  /** System preferences (see @ref services_blob_db_prefs_db). */
  BlobDBIdPrefs = 0x07,
  /** Contacts (see @ref services_blob_db_contacts_db). */
  BlobDBIdContacts = 0x08,
  /** Watch app preferences (see @ref services_blob_db_watch_app_prefs_db). */
  BlobDBIdWatchAppPrefs = 0x09,
  /** Health typicals and averages (see @ref services_blob_db_health_db). */
  BlobDBIdHealth = 0x0A,
  /** App glances (see @ref services_blob_db_app_glance_db). */
  BlobDBIdAppGlance = 0x0B,
  /** Synced settings (see @ref services_blob_db_settings_blob_db). */
  BlobDBIdSettings = 0x0C,
  /** Number of database ids. */
  NumBlobDBs,
} BlobDBId;
_Static_assert(sizeof(BlobDBId) == 1, "BlobDBId is larger than 1 byte");

/**
 * @brief Node of a list of records that have not been synced to the phone yet.
 *
 * Allocated with the key appended; free a whole list with blob_db_util_free_dirty_list().
 */
typedef struct {
  /** List node. */
  ListNode node;
  /** Time the record was last modified. */
  time_t last_updated;
  /** Length of @ref key in bytes. */
  int key_len;
  /** Key data. */
  uint8_t key[];
} BlobDBDirtyItem;

/**
 * @brief Initialize a database.
 *
 * Called once at boot by blob_db_init_dbs().
 */
typedef void (*BlobDBInitImpl)(void);

/**
 * @brief Insert or replace a record.
 *
 * Blocking.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise (see @c StatusCode).
 */
typedef status_t (*BlobDBInsertImpl)(const uint8_t *key, int key_len, const uint8_t *val,
                                     int val_len);

/**
 * @brief Get the length of a record's value.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length of the value in bytes, 0 if the key does not exist, or a negative error code.
 */
typedef int (*BlobDBGetLenImpl)(const uint8_t *key, int key_len);

/**
 * @brief Read a record's value.
 *
 * Blocking.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer of @p val_len bytes.
 * @param val_len Number of bytes to copy.
 * @return @c S_SUCCESS on success, an error code otherwise (see @c StatusCode).
 */
typedef status_t (*BlobDBReadImpl)(const uint8_t *key, int key_len, uint8_t *val_out, int val_len);

/**
 * @brief Delete a record.
 *
 * Blocking.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise (see @c StatusCode).
 */
typedef status_t (*BlobDBDeleteImpl)(const uint8_t *key, int key_len);

/**
 * @brief Delete all records.
 *
 * Blocking.
 *
 * @return @c S_SUCCESS on success, an error code otherwise (see @c StatusCode).
 */
typedef status_t (*BlobDBFlushImpl)(void);

/**
 * @brief Check whether the database holds records not yet synced to the phone.
 *
 * @param[out] is_dirty_out Set to true if there is at least one dirty record. Undefined on
 * failure.
 * @return @c S_SUCCESS if the query succeeded, an error code otherwise.
 */
typedef status_t (*BlobDBIsDirtyImpl)(bool *is_dirty_out);

/**
 * @brief Build the list of records not yet synced to the phone.
 *
 * The list size is unbounded; it may be incomplete when memory runs out.
 *
 * @return Heap-allocated list with one node per dirty record, NULL if there is none.
 */
typedef BlobDBDirtyItem *(*BlobDBGetDirtyListImpl)(void);

/**
 * @brief Mark a record as synced.
 *
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
typedef status_t (*BlobDBMarkSyncedImpl)(const uint8_t *key, int key_len);

/**
 * @brief Reclaim unused space in the backing settings file.
 *
 * Blocking. Only databases backed by a settings file implement it.
 *
 * @return @c S_SUCCESS on success, an error code otherwise.
 */
typedef status_t (*BlobDBCompactImpl)(void);

/**
 * @brief Emit a @c PEBBLE_BLOBDB_EVENT.
 *
 * The key is copied into a kernel heap buffer owned by the event.
 *
 * @param type Event type.
 * @param db_id Database the event refers to.
 * @param key Key data, may be NULL when @p key_len is 0.
 * @param key_len Length of @p key in bytes.
 */
void blob_db_event_put(BlobDBEventType type, BlobDBId db_id, const uint8_t *key, int key_len);

/** @brief Call the init callback of every database. */
void blob_db_init_dbs(void);

/**
 * @brief Compact every database that implements ::BlobDBCompactImpl.
 *
 * Must be called after blob_db_init_dbs(). Performs flash I/O: call it from a system task
 * callback, not from the kernel main loop.
 */
void blob_db_compact_growable_dbs(void);

/**
 * @brief List the databases that hold records not yet synced to the phone.
 *
 * @param[out] ids Array of at least ::NumBlobDBs entries, filled with dirty ::BlobDBId values.
 * @param[out] num_ids Number of entries written to @p ids.
 */
void blob_db_get_dirty_dbs(uint8_t *ids, uint8_t *num_ids);

/**
 * @brief Insert or replace a record in a database.
 *
 * Emits a ::BlobDBEventTypeInsert event on success.
 *
 * @param db_id Database.
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param val Value data.
 * @param val_len Length of @p val in bytes.
 * @retval S_SUCCESS Inserted.
 * @retval E_RANGE Invalid or disabled database.
 * @retval E_INVALID_OPERATION Operation not supported by the database.
 * @return Other error codes from ::BlobDBInsertImpl.
 */
status_t blob_db_insert(BlobDBId db_id, const uint8_t *key, int key_len, const uint8_t *val,
                        int val_len);

/**
 * @brief Get the length of a record's value.
 *
 * @param db_id Database.
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @return Length in bytes, 0 if the key does not exist, or a negative error code
 * (@c E_RANGE for an invalid database, @c E_INVALID_OPERATION if unsupported).
 */
int blob_db_get_len(BlobDBId db_id, const uint8_t *key, int key_len);

/**
 * @brief Read a record's value.
 *
 * @param db_id Database.
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Buffer of @p val_len bytes.
 * @param val_len Number of bytes to copy, usually from blob_db_get_len().
 * @retval S_SUCCESS Read.
 * @retval E_RANGE Invalid or disabled database.
 * @retval E_INVALID_OPERATION Operation not supported by the database.
 * @return Other error codes from ::BlobDBReadImpl.
 */
status_t blob_db_read(BlobDBId db_id, const uint8_t *key, int key_len, uint8_t *val_out,
                      int val_len);

/**
 * @brief Delete a record.
 *
 * Emits a ::BlobDBEventTypeDelete event on success.
 *
 * @param db_id Database.
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @retval S_SUCCESS Deleted.
 * @retval E_RANGE Invalid or disabled database.
 * @retval E_INVALID_OPERATION Operation not supported by the database.
 * @return Other error codes from ::BlobDBDeleteImpl.
 */
status_t blob_db_delete(BlobDBId db_id, const uint8_t *key, int key_len);

/**
 * @brief Delete all records of a database.
 *
 * Emits a ::BlobDBEventTypeFlush event on success.
 *
 * @param db_id Database.
 * @retval S_SUCCESS Flushed.
 * @retval E_RANGE Invalid or disabled database.
 * @retval E_INVALID_OPERATION Operation not supported by the database.
 * @return Other error codes from ::BlobDBFlushImpl.
 */
status_t blob_db_flush(BlobDBId db_id);

/**
 * @brief Get the records of a database that have not been synced to the phone.
 *
 * Records written by the phone are always marked as synced. Use the API in
 * @ref services_blob_db_sync to start a sync.
 *
 * @param db_id Database.
 * @return Heap-allocated list (free with blob_db_util_free_dirty_list()), or NULL if there are
 * no dirty records or the database does not track them.
 */
BlobDBDirtyItem *blob_db_get_dirty_list(BlobDBId db_id);

/**
 * @brief Mark a record as synced.
 *
 * Used when the phone acknowledges a write during a sync.
 *
 * @param db_id Database.
 * @param key Key data.
 * @param key_len Length of @p key in bytes.
 * @retval S_SUCCESS Marked.
 * @retval E_RANGE Invalid or disabled database.
 * @retval E_INVALID_OPERATION Operation not supported by the database.
 * @return Other error codes from ::BlobDBMarkSyncedImpl.
 */
status_t blob_db_mark_synced(BlobDBId db_id, uint8_t *key, int key_len);

/** @} */
