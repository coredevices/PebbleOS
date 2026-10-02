/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "settings_raw_iter.h"

#include <time.h>
#include "pbl/util/units.h"

/**
 * @defgroup services_settings Settings files
 * @ingroup services
 * @brief Atomic key-value store on top of PFS.
 *
 * A settings file is a log-structured binary key-value store kept in a single PFS file. Keys
 * (up to @ref SETTINGS_KEY_MAX_LEN bytes) and values (up to @ref SETTINGS_VAL_MAX_LEN bytes) are
 * arbitrary bytes. Every update is atomic: after a reboot in the middle of a write, the record
 * holds either the old or the new value. Records carry a modification timestamp and a synced
 * flag used for synchronization with the phone.
 *
 * Settings files are not thread-safe: callers serialize access to a file themselves, and a file
 * must not be open twice at the same time.
 *
 * @code{.c}
 * SettingsFile file;
 * uint32_t key = 1;
 * uint8_t value = 42;
 *
 * if (settings_file_open(&file, "myprefs", 1024) == S_SUCCESS) {
 *   settings_file_set(&file, &key, sizeof(key), &value, sizeof(value));
 *   settings_file_get(&file, &key, sizeof(key), &value, sizeof(value));
 *   settings_file_close(&file);
 * }
 * @endcode
 * @{
 */

/**
 * @brief Minimum time in seconds a deleted record is kept before compaction drops it.
 *
 * Keeping the key around gives the deletion time to propagate to synchronized devices.
 */
#define DELETED_LIFETIME (0 * PBL_SEC_PER_DAY)

/**
 * @brief Open settings file.
 *
 * The fields are internal to the settings file implementation.
 */
typedef struct SettingsFile {
  /** Raw iterator over the underlying file. */
  SettingsRawIter iter;
  /** File name, heap allocated. */
  char *name;

  /** Space the file may take before a compaction is forced, at least @ref max_used_space. */
  int max_space_total;

  /** Space valid records may use; writes beyond it fail with @c E_OUT_OF_STORAGE. */
  int max_used_space;

  /**
   * Current allocation budget. Grows from the initial size toward @ref max_used_space for growable
   * files, equals @ref max_used_space otherwise.
   */
  int alloc_used_space;

  /** Lower bound of @ref alloc_used_space when compacting. */
  int min_alloc_used_space;

  /** Space taken by overwritten records, reclaimed by compaction. */
  int dead_space;

  /** Space taken by valid records. */
  int used_space;

  /** Most recent modification time of any record. */
  uint32_t last_modified;

  /**
   * Position of the record being visited by settings_file_each() or settings_file_rewrite(), so
   * that other records can be read from the callback. 0 when not iterating.
   */
  int cur_record_pos;
} SettingsFile;

/**
 * @brief Open or create a settings file.
 *
 * Corrupt files, files with an unknown version and files whose recovery fails are removed and
 * recreated empty. A file created with a smaller size is rewritten to the requested size.
 *
 * Persist files need @p max_used_space of at least 5317 bytes to always fit all records in the
 * worst case (all values booleans).
 *
 * @param[out] file Settings file to initialize.
 * @param name PFS file name.
 * @param max_used_space Space valid records may use, in bytes.
 * @retval S_SUCCESS File opened.
 * @return Negative status code from PFS on failure.
 */
status_t settings_file_open(SettingsFile *file, const char *name, int max_used_space);

/**
 * @brief Open or create a settings file that grows on demand.
 *
 * Like settings_file_open(), but the file starts with room for @p initial_alloc_size bytes of
 * records and doubles its allocation as needed, up to @p max_used_space.
 *
 * @param[out] file Settings file to initialize.
 * @param name PFS file name.
 * @param max_used_space Space valid records may use, in bytes.
 * @param initial_alloc_size Initial allocation in bytes, greater than 0.
 * @retval S_SUCCESS File opened.
 * @return Negative status code from PFS on failure.
 */
status_t settings_file_open_growable(SettingsFile *file, const char *name, int max_used_space,
                                     int initial_alloc_size);

/**
 * @brief Close a settings file.
 *
 * @param file Settings file.
 */
void settings_file_close(SettingsFile *file);

/**
 * @brief Check whether a key holds a non-empty value.
 *
 * @param file Settings file.
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @return true if the record exists and is not deleted.
 */
bool settings_file_exists(SettingsFile *file, const void *key, size_t key_len);

/**
 * @brief Delete a record.
 *
 * Writes an empty value, which marks the record as deleted.
 *
 * @param file Settings file.
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @return Same as settings_file_set().
 */
status_t settings_file_delete(SettingsFile *file, const void *key, size_t key_len);

/**
 * @brief Get the length of a value.
 *
 * @param file Settings file.
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @return Value length in bytes, 0 if the record does not exist or is deleted.
 */
int settings_file_get_len(SettingsFile *file, const void *key, size_t key_len);

/**
 * @brief Read a value.
 *
 * Reads the first @p val_out_len bytes of the value. On failure @p val_out is zeroed.
 *
 * @param file Settings file.
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @param[out] val_out Value buffer.
 * @param val_out_len Bytes to read, at most the stored value length.
 * @retval S_SUCCESS Value read.
 * @retval E_DOES_NOT_EXIST No such record, or it was deleted.
 * @retval E_RANGE @p val_out_len exceeds the stored value length.
 */
status_t settings_file_get(SettingsFile *file, const void *key, size_t key_len, void *val_out,
                           size_t val_out_len);

/**
 * @brief Write a value, timestamped with the current time.
 *
 * Atomic: after a reboot the record holds either the old or the new value. Compacts or grows the
 * file when needed. Must not be called from a settings_file_each() callback. Invokes the change
 * callback, if any, on success.
 *
 * @param file Settings file.
 * @param key Key, at most @ref SETTINGS_KEY_MAX_LEN bytes.
 * @param key_len Length of @p key in bytes.
 * @param val Value.
 * @param val_len Length of @p val in bytes, at most @ref SETTINGS_VAL_MAX_LEN. 0 deletes the
 * record.
 * @retval S_SUCCESS Value written.
 * @retval E_RANGE Key or value too long.
 * @retval E_OUT_OF_STORAGE Not enough space for valid records.
 * @return Other negative status codes if compaction fails.
 */
status_t settings_file_set(SettingsFile *file, const void *key, size_t key_len, const void *val,
                           size_t val_len);

/**
 * @brief Write a value with a given timestamp instead of the current time.
 *
 * Used when rewriting files to preserve the original timestamps.
 *
 * @param file Settings file.
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @param val Value.
 * @param val_len Length of @p val in bytes.
 * @param timestamp Modification time to record.
 * @return Same as settings_file_set().
 */
status_t settings_file_set_with_timestamp(SettingsFile *file, const void *key, size_t key_len,
                                          const void *val, size_t val_len, uint32_t timestamp);

/**
 * @brief Mark a record as synced.
 *
 * The flag stays until the record is overwritten.
 *
 * @param file Settings file.
 * @param key Key, at most @ref SETTINGS_KEY_MAX_LEN bytes.
 * @param key_len Length of @p key in bytes.
 * @retval S_SUCCESS Record marked.
 * @retval E_RANGE Key too long.
 * @retval E_DOES_NOT_EXIST No such record.
 */
status_t settings_file_mark_synced(SettingsFile *file, const void *key, size_t key_len);

/**
 * @brief Mark all records as not synced, to trigger a full sync.
 *
 * Rewrites the whole file, preserving timestamps, which can be slow for large files.
 *
 * @param file Settings file.
 * @return Same as settings_file_rewrite().
 */
status_t settings_file_mark_all_dirty(SettingsFile *file);

/**
 * @brief Callback invoked after a record is written.
 *
 * @param file Settings file that was modified.
 * @param key Key that was written.
 * @param key_len Length of @p key in bytes.
 * @param last_modified Timestamp of the change.
 */
typedef void (*SettingsFileChangeCallback)(SettingsFile *file, const void *key, int key_len,
                                           time_t last_modified);

/**
 * @brief Register the callback invoked after any record of any settings file is written.
 *
 * Only one callback is supported; registering replaces the previous one.
 *
 * @param callback Callback, or NULL to unregister.
 */
void settings_file_set_change_callback(SettingsFileChangeCallback callback);

/**
 * @brief Write a single byte of a value in place.
 *
 * Writes flash directly, so it can only clear bits. Only atomic for a single byte: do not use it
 * to modify several bytes in a row.
 *
 * @param file Settings file.
 * @param key Key, at most @ref SETTINGS_KEY_MAX_LEN bytes.
 * @param key_len Length of @p key in bytes.
 * @param offset Offset within the value, less than the value length.
 * @param byte Byte to write.
 * @retval S_SUCCESS Byte written.
 * @retval E_RANGE Key too long.
 * @retval E_DOES_NOT_EXIST No such record, or it was deleted.
 */
status_t settings_file_set_byte(SettingsFile *file, const void *key, size_t key_len, size_t offset,
                                uint8_t byte);

/**
 * @brief Read the key or value of the record being visited.
 *
 * @param file Settings file.
 * @param[out] buf Destination buffer.
 * @param buf_len Bytes to read, at most the key or value length.
 */
typedef void (*SettingsFileGetter)(SettingsFile *file, void *buf, size_t buf_len);

/** @brief Record being visited by settings_file_each() or settings_file_rewrite(). */
typedef struct {
  /** Modification timestamp. */
  uint32_t last_modified;
  /** Reads the key. */
  SettingsFileGetter get_key;
  /** Key length in bytes. */
  int key_len;
  /** Reads the value. */
  SettingsFileGetter get_val;
  /** Value length in bytes, 0 for a deleted record. */
  int val_len;
  /** Record has not been marked as synced. */
  bool dirty;
} SettingsRecordInfo;

/**
 * @brief Callback for settings_file_each().
 *
 * @param file Settings file being iterated.
 * @param info Current record.
 * @param context Context passed to settings_file_each().
 * @return true to continue iterating, false to stop.
 */
typedef bool (*SettingsFileEachCallback)(SettingsFile *file, SettingsRecordInfo *info,
                                         void *context);

/**
 * @brief Call a callback for every valid record.
 *
 * The callback may read other records but must not modify the file; use
 * settings_file_rewrite() for that.
 *
 * @param file Settings file.
 * @param cb Callback.
 * @param context Context passed to @p cb.
 * @retval S_SUCCESS Always.
 */
status_t settings_file_each(SettingsFile *file, SettingsFileEachCallback cb, void *context);

/**
 * @brief Callback for settings_file_rewrite().
 *
 * @param old_file Original file, to read the record from.
 * @param new_file New file, to write records that should be kept to.
 * @param info Current record of @p old_file.
 * @param context Context passed to settings_file_rewrite().
 */
typedef void (*SettingsFileRewriteCallback)(SettingsFile *old_file, SettingsFile *new_file,
                                            SettingsRecordInfo *info, void *context);

/**
 * @brief Rewrite a settings file record by record.
 *
 * Opens a new file with the same name in overwrite mode and calls @p cb for each record of the
 * original one. Only records the callback writes to the new file are kept. The new file replaces
 * the original atomically, and @p file is reopened on it.
 *
 * @param file Settings file.
 * @param cb Callback.
 * @param context Context passed to @p cb.
 * @retval S_SUCCESS File rewritten.
 * @retval E_OUT_OF_MEMORY Allocation failed.
 * @return Other negative status codes if opening the new file fails.
 */
status_t settings_file_rewrite(SettingsFile *file, SettingsFileRewriteCallback cb, void *context);

/**
 * @brief Filter callback for settings_file_rewrite_filtered().
 *
 * Must not call any other settings file function.
 *
 * @param key Record key.
 * @param key_len Length of @p key in bytes.
 * @param value Record value.
 * @param value_len Length of @p value in bytes.
 * @param context Context passed to settings_file_rewrite_filtered().
 * @return true to keep the record, false to drop it.
 */
typedef bool (*SettingsFileRewriteFilterCallback)(void *key, size_t key_len, void *value,
                                                  size_t value_len, void *context);

/**
 * @brief Rewrite a settings file, keeping only the records a filter accepts.
 *
 * Much faster than settings_file_rewrite() when records are only being dropped.
 *
 * @param file Settings file.
 * @param filter_cb Filter, or NULL to keep every valid record.
 * @param context Context passed to @p filter_cb.
 * @retval S_SUCCESS File rewritten.
 * @retval E_OUT_OF_MEMORY Allocation failed.
 * @return Other negative status codes on failure.
 */
status_t settings_file_rewrite_filtered(SettingsFile *file,
                                        SettingsFileRewriteFilterCallback filter_cb, void *context);

/**
 * @brief Compact a settings file.
 *
 * Rewrites all valid records, dropping dead space. Growable files shrink their allocation toward
 * @ref SettingsFile::min_alloc_used_space.
 *
 * @param file Settings file.
 * @retval S_SUCCESS File compacted.
 * @return Negative status code on failure.
 */
status_t settings_file_compact(SettingsFile *file);

/** @} */
