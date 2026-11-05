/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup settings Settings files
 * @ingroup subsys
 * @brief Atomic key-value store on top of PFS.
 *
 * A settings file is a log-structured binary key-value store kept in a single PFS file. Keys
 * (up to @ref PBL_SETTINGS_KEY_MAX_LEN bytes) and values (up to @ref PBL_SETTINGS_VAL_MAX_LEN
 * bytes) are arbitrary bytes. Every update is atomic: after a reboot in the middle of a write, the
 * record holds either the old or the new value. Records carry a modification timestamp and a synced
 * flag used for synchronization with the phone.
 *
 * Settings files are not thread-safe: callers serialize access to a file themselves, and a file
 * must not be open twice at the same time.
 *
 * @code{.c}
 * struct pbl_settings_file file;
 * uint32_t key = 1;
 * uint8_t value = 42;
 *
 * if (pbl_settings_open(&file, "myprefs", 1024) == 0) {
 *   pbl_settings_set(&file, &key, sizeof(key), &value, sizeof(value));
 *   pbl_settings_get(&file, &key, sizeof(key), &value, sizeof(value));
 *   pbl_settings_close(&file);
 * }
 * @endcode
 * @{
 */

/** @brief Maximum key length in bytes. */
#define PBL_SETTINGS_KEY_MAX_LEN 127
/** @brief Maximum value length in bytes. */
#define PBL_SETTINGS_VAL_MAX_LEN 2046

/** @cond INTERNAL_HIDDEN */

/** @brief Settings file header. */
struct PBL_PACKED pbl_settings_file_hdr {
  /** File magic. */
  uint32_t magic;
  /** Format version. */
  uint16_t version;
  /** Unused, all ones. */
  uint16_t flags;
};

/** @brief Record header. */
struct PBL_PACKED pbl_settings_record_hdr {
  /** Modification time, UTC seconds. */
  uint32_t last_modified;
  /** pbl_crc8_reversed() of the key, to skip non-matching records quickly. */
  uint8_t key_hash;
  /** Active-low flags. */
  uint8_t flags : 6;
  /** Key length in bytes. */
  unsigned int key_len : 7;
  /** Value length in bytes, 0 for a deleted record. */
  unsigned int val_len : 11;
};

/**
 * @brief Cursor over the records of a settings file.
 *
 * Guarantees the upper layers never lose track of their position in the file (reading data as a
 * header, or past the end of a key or value), and turns unexpected conditions from bad logic or
 * corruption into a controlled failure.
 */
struct pbl_settings_raw_iter {
  /** File name, for diagnostics. */
  const char *file_name;
  /** Open file. */
  struct pbl_fs_file file;
  /** File header. */
  struct pbl_settings_file_hdr file_hdr;

  /** Header of the current record. */
  struct pbl_settings_record_hdr hdr;
  /** Offset of the current record header. */
  int hdr_pos;
  /**
   * Offset of the record a search began or resumed from, so a search can wrap around from the
   * end to the beginning. Only changed by pbl_settings__raw_iter_begin() and
   * pbl_settings__raw_iter_resume().
   */
  int resumed_pos;
};

/** @endcond */

/**
 * @brief Open settings file.
 *
 * The fields are internal to the settings file implementation.
 */
struct pbl_settings_file {
  /** Raw iterator over the underlying file. */
  struct pbl_settings_raw_iter iter;
  /** File name, heap allocated. */
  char *name;

  /** Space the file may take before a compaction is forced, at least @ref max_used_space. */
  int max_space_total;

  /** Space valid records may use; writes beyond it fail with @c -ENOSPC. */
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
   * Position of the record being visited by pbl_settings_each() or pbl_settings_rewrite(), so
   * that other records can be read from the callback. 0 when not iterating.
   */
  int cur_record_pos;
};

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
 * @param name Absolute path of the file, on PFS.
 * @param max_used_space Space valid records may use, in bytes.
 * @retval 0 File opened.
 * @return Negative errno from the file system on failure.
 */
int pbl_settings_open(struct pbl_settings_file *file, const char *name, int max_used_space);

/**
 * @brief Open or create a settings file that grows on demand.
 *
 * Like pbl_settings_open(), but the file starts with room for @p initial_alloc_size bytes of
 * records and doubles its allocation as needed, up to @p max_used_space.
 *
 * @param[out] file Settings file to initialize.
 * @param name Absolute path of the file, on PFS.
 * @param max_used_space Space valid records may use, in bytes.
 * @param initial_alloc_size Initial allocation in bytes, greater than 0.
 * @retval 0 File opened.
 * @return Negative errno from the file system on failure.
 */
int pbl_settings_open_growable(struct pbl_settings_file *file, const char *name, int max_used_space,
                               int initial_alloc_size);

/**
 * @brief Close a settings file.
 *
 * @param file Settings file.
 */
void pbl_settings_close(struct pbl_settings_file *file);

/**
 * @brief Check whether a key holds a non-empty value.
 *
 * @param file Settings file.
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @return true if the record exists and is not deleted.
 */
bool pbl_settings_exists(struct pbl_settings_file *file, const void *key, size_t key_len);

/**
 * @brief Delete a record.
 *
 * Writes an empty value, which marks the record as deleted.
 *
 * @param file Settings file.
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @return Same as pbl_settings_set().
 */
int pbl_settings_delete(struct pbl_settings_file *file, const void *key, size_t key_len);

/**
 * @brief Get the length of a value.
 *
 * @param file Settings file.
 * @param key Key.
 * @param key_len Length of @p key in bytes.
 * @return Value length in bytes, 0 if the record does not exist or is deleted.
 */
size_t pbl_settings_get_len(struct pbl_settings_file *file, const void *key, size_t key_len);

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
 * @retval 0 Value read.
 * @retval -ENOENT No such record, or it was deleted.
 * @retval -ERANGE @p val_out_len exceeds the stored value length.
 */
int pbl_settings_get(struct pbl_settings_file *file, const void *key, size_t key_len, void *val_out,
                     size_t val_out_len);

/**
 * @brief Write a value, timestamped with the current time.
 *
 * Atomic: after a reboot the record holds either the old or the new value. Compacts or grows the
 * file when needed. Must not be called from a pbl_settings_each() callback. Invokes the change
 * callback, if any, on success.
 *
 * @param file Settings file.
 * @param key Key, at most @ref PBL_SETTINGS_KEY_MAX_LEN bytes.
 * @param key_len Length of @p key in bytes.
 * @param val Value.
 * @param val_len Length of @p val in bytes, at most @ref PBL_SETTINGS_VAL_MAX_LEN. 0 deletes the
 * record.
 * @retval 0 Value written.
 * @retval -ERANGE Key or value too long.
 * @retval -ENOSPC Not enough space for valid records.
 * @return Other negative errno values if compaction fails.
 */
int pbl_settings_set(struct pbl_settings_file *file, const void *key, size_t key_len,
                     const void *val, size_t val_len);

/**
 * @brief Mark a record as synced.
 *
 * The flag stays until the record is overwritten.
 *
 * @param file Settings file.
 * @param key Key, at most @ref PBL_SETTINGS_KEY_MAX_LEN bytes.
 * @param key_len Length of @p key in bytes.
 * @retval 0 Record marked.
 * @retval -ERANGE Key too long.
 * @retval -ENOENT No such record.
 */
int pbl_settings_mark_synced(struct pbl_settings_file *file, const void *key, size_t key_len);

/**
 * @brief Mark all records as not synced, to trigger a full sync.
 *
 * Rewrites the whole file, preserving timestamps, which can be slow for large files.
 *
 * @param file Settings file.
 * @return Same as pbl_settings_rewrite().
 */
int pbl_settings_mark_all_dirty(struct pbl_settings_file *file);

/**
 * @brief Callback invoked after a record of a watched file is written.
 *
 * Runs with the file still open: it must not open the same file again.
 *
 * @param key Key that was written.
 * @param key_len Length of @p key in bytes.
 * @param last_modified Timestamp of the change.
 * @param user_data User data of the watch.
 */
typedef void (*pbl_settings_change_cb_t)(const void *key, size_t key_len, uint32_t last_modified,
                                         void *user_data);

/** @brief Watch on the changes to a settings file. */
struct pbl_settings_watch {
  /** Name of the watched file, as passed to pbl_settings_open(). */
  const char *name;
  /** Callback. */
  pbl_settings_change_cb_t cb;
  /** User data passed to @ref cb. */
  void *user_data;
  /** Next watch, internal. */
  struct pbl_settings_watch *next;
};

/**
 * @brief Register a watch on the changes to a settings file.
 *
 * Meant to be called at initialization, before the file is written. The watch must stay valid
 * forever. Several watches may be registered, also on the same file.
 *
 * @param watch Watch, with @ref pbl_settings_watch::name and @ref pbl_settings_watch::cb set.
 */
void pbl_settings_watch_add(struct pbl_settings_watch *watch);

/**
 * @brief Write a single byte of a value in place.
 *
 * Writes flash directly, so it can only clear bits. Only atomic for a single byte: do not use it
 * to modify several bytes in a row.
 *
 * @param file Settings file.
 * @param key Key, at most @ref PBL_SETTINGS_KEY_MAX_LEN bytes.
 * @param key_len Length of @p key in bytes.
 * @param offset Offset within the value, less than the value length.
 * @param byte Byte to write.
 * @retval 0 Byte written.
 * @retval -ERANGE Key too long.
 * @retval -ENOENT No such record, or it was deleted.
 */
int pbl_settings_set_byte(struct pbl_settings_file *file, const void *key, size_t key_len,
                          size_t offset, uint8_t byte);

/**
 * @brief Read the key of the record being visited.
 *
 * Only valid from a pbl_settings_each() or pbl_settings_rewrite() callback.
 *
 * @param file Settings file being iterated.
 * @param[out] buf Destination buffer.
 * @param buf_len Bytes to read, at most the key length.
 */
void pbl_settings_read_key(struct pbl_settings_file *file, void *buf, size_t buf_len);

/**
 * @brief Read the value of the record being visited.
 *
 * Only valid from a pbl_settings_each() or pbl_settings_rewrite() callback.
 *
 * @param file Settings file being iterated.
 * @param[out] buf Destination buffer.
 * @param buf_len Bytes to read, at most the value length.
 */
void pbl_settings_read_val(struct pbl_settings_file *file, void *buf, size_t buf_len);

/** @brief Record being visited by pbl_settings_each() or pbl_settings_rewrite(). */
struct pbl_settings_record {
  /** Modification timestamp. */
  uint32_t last_modified;
  /** Key length in bytes. */
  size_t key_len;
  /** Value length in bytes, 0 for a deleted record. */
  size_t val_len;
  /** Record has not been marked as synced. */
  bool dirty;
};

/**
 * @brief Callback for pbl_settings_each().
 *
 * @param file Settings file being iterated.
 * @param info Current record.
 * @param context Context passed to pbl_settings_each().
 * @return true to continue iterating, false to stop.
 */
typedef bool (*pbl_settings_each_cb_t)(struct pbl_settings_file *file,
                                       struct pbl_settings_record *info, void *context);

/**
 * @brief Call a callback for every valid record.
 *
 * The callback may read other records but must not modify the file; use
 * pbl_settings_rewrite() for that.
 *
 * @param file Settings file.
 * @param cb Callback.
 * @param context Context passed to @p cb.
 */
void pbl_settings_each(struct pbl_settings_file *file, pbl_settings_each_cb_t cb, void *context);

/**
 * @brief Callback for pbl_settings_rewrite().
 *
 * @param old_file Original file, to read the record from.
 * @param new_file New file, to write records that should be kept to.
 * @param info Current record of @p old_file.
 * @param context Context passed to pbl_settings_rewrite().
 */
typedef void (*pbl_settings_rewrite_cb_t)(struct pbl_settings_file *old_file,
                                          struct pbl_settings_file *new_file,
                                          struct pbl_settings_record *info, void *context);

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
 * @retval 0 File rewritten.
 * @retval -ENOMEM Allocation failed.
 * @return Other negative errno values if opening the new file fails.
 */
int pbl_settings_rewrite(struct pbl_settings_file *file, pbl_settings_rewrite_cb_t cb,
                         void *context);

/**
 * @brief Filter callback for pbl_settings_rewrite_filtered().
 *
 * Must not call any other settings file function.
 *
 * @param key Record key.
 * @param key_len Length of @p key in bytes.
 * @param value Record value.
 * @param value_len Length of @p value in bytes.
 * @param context Context passed to pbl_settings_rewrite_filtered().
 * @return true to keep the record, false to drop it.
 */
typedef bool (*pbl_settings_filter_cb_t)(void *key, size_t key_len, void *value, size_t value_len,
                                         void *context);

/**
 * @brief Rewrite a settings file, keeping only the records a filter accepts.
 *
 * Much faster than pbl_settings_rewrite() when records are only being dropped.
 *
 * @param file Settings file.
 * @param filter_cb Filter, or NULL to keep every valid record.
 * @param context Context passed to @p filter_cb.
 * @retval 0 File rewritten.
 * @retval -ENOMEM Allocation failed.
 * @return Other negative errno values on failure.
 */
int pbl_settings_rewrite_filtered(struct pbl_settings_file *file,
                                  pbl_settings_filter_cb_t filter_cb, void *context);

/**
 * @brief Compact a settings file.
 *
 * Rewrites all valid records, dropping dead space. Growable files shrink their allocation toward
 * @ref pbl_settings_file::min_alloc_used_space.
 *
 * @param file Settings file.
 * @retval 0 File compacted.
 * @return Negative errno on failure.
 */
int pbl_settings_compact(struct pbl_settings_file *file);

/** @} */
