/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include <pbl/kernel/compiler.h>

#include <system/status_codes.h>

/**
 * @defgroup services_settings_settings_raw_iter Settings file raw iterator
 * @ingroup services_settings
 * @brief On-flash format of settings files and a cursor over their records.
 *
 * Internal to the settings file implementation; use settings_file_each() instead.
 *
 * A settings file is a @ref SettingsFileHeader followed by records, each a
 * @ref SettingsRecordHeader, the key and the value. Unwritten flash (all ones) marks the end.
 * Record flags are active low: a flag is set by clearing its bit, so it can be written in place.
 *
 * Any PFS error during iteration is treated as a fatal logic error: the file is removed and the
 * system reboots, so that the corruption cannot cause a reboot loop.
 * @{
 */

/** @brief Magic at the start of every settings file. */
#define SETTINGS_FILE_MAGIC "set"
/** @brief Current settings file format version. */
#define SETTINGS_FILE_VERSION 1

/** @brief Settings file header. */
typedef struct PBL_PACKED {
  /** @ref SETTINGS_FILE_MAGIC, including the NUL terminator. */
  uint32_t magic;
  /** Format version, see @ref SETTINGS_FILE_VERSION. */
  uint16_t version;
  /** Unused, all ones. */
  uint16_t flags;
} SettingsFileHeader;

static_assert(sizeof((SettingsFileHeader){}.magic) == sizeof(SETTINGS_FILE_MAGIC),
              "The magic has been broken!");

/** @brief Record header, key and value are completely written. */
#define SETTINGS_FLAG_WRITE_COMPLETE (1 << 0)
/** @brief A newer record for the same key is being written. */
#define SETTINGS_FLAG_OVERWRITE_STARTED (1 << 1)
/** @brief A newer record for the same key is complete; this one is dead. */
#define SETTINGS_FLAG_OVERWRITE_COMPLETE (1 << 2)
/** @brief Record is in sync with the phone. */
#define SETTINGS_FLAG_SYNCED (1 << 3)

/** @brief Maximum key length in bytes. */
#define SETTINGS_KEY_MAX_LEN 127
/** @brief Maximum value length in bytes. */
#define SETTINGS_VAL_MAX_LEN (SETTINGS_EOF_MARKER - 1) // we reserve the largest value for EOF

/** @brief Width of the key length field of a record header. */
#define KEY_LEN_BITS 7
/** @brief Width of the value length field of a record header. */
#define VAL_LEN_BITS 11
/** @brief Width of the flags field of a record header. */
#define FLAGS_BITS 6

/** @brief Value length of the all-ones header that marks the end of the records. */
#define SETTINGS_EOF_MARKER ((1 << VAL_LEN_BITS) - 1)

static_assert(KEY_LEN_BITS + VAL_LEN_BITS + FLAGS_BITS == 24,
              "The record header bitfields must add up to 24!");

/** @brief Record header. */
typedef struct PBL_PACKED {
  /** Modification time, UTC seconds. */
  uint32_t last_modified;
  /** pbl_crc8_reversed() of the key, to skip non-matching records quickly. */
  uint8_t key_hash;
  /** Active-low @c SETTINGS_FLAG_* bits. */
  uint8_t flags : FLAGS_BITS;
  /** Key length in bytes. */
  unsigned int key_len : KEY_LEN_BITS;
  /** Value length in bytes, 0 for a deleted record. */
  unsigned int val_len : VAL_LEN_BITS;
} SettingsRecordHeader;

/**
 * @brief Cursor over the records of a settings file.
 *
 * Guarantees the upper layers never lose track of their position in the file (reading data as a
 * header, or past the end of a key or value), and turns unexpected conditions from bad logic or
 * corruption into a controlled failure.
 */
typedef struct {
  /** File name, for diagnostics. */
  const char *file_name;
  /** PFS file descriptor. */
  int fd;
  /** File header. */
  SettingsFileHeader file_hdr;

  /** Header of the current record. */
  SettingsRecordHeader hdr;
  /** Offset of the current record header. */
  int hdr_pos;
  /**
   * Offset of the record a search began or resumed from, so a search can wrap around from the
   * end to the beginning. Only changed by settings_raw_iter_begin() and
   * settings_raw_iter_resume().
   */
  int resumed_pos;
} SettingsRawIter;

/**
 * @brief Initialize an iterator and read the file header.
 *
 * @param[out] iter Iterator.
 * @param fd Open PFS file.
 * @param file_name File name, for diagnostics. Must outlive the iterator.
 */
void settings_raw_iter_init(SettingsRawIter *iter, int fd, const char *file_name);

/**
 * @brief Write the file header, for newly created files.
 *
 * @param iter Iterator.
 * @param file_hdr Header to write.
 */
void settings_raw_iter_write_file_header(SettingsRawIter *iter, SettingsFileHeader *file_hdr);

/**
 * @brief Move to the first record.
 *
 * @param iter Iterator.
 */
void settings_raw_iter_begin(SettingsRawIter *iter);

/**
 * @brief Start a search from the current record.
 *
 * @param iter Iterator.
 */
void settings_raw_iter_resume(SettingsRawIter *iter);

/**
 * @brief Move to the next record.
 *
 * @param iter Iterator.
 */
void settings_raw_iter_next(SettingsRawIter *iter);

/**
 * @brief Check whether the iterator is past the last record.
 *
 * @param iter Iterator.
 * @return true at the end of the records.
 */
bool settings_raw_iter_end(SettingsRawIter *iter);

/**
 * @brief Get the current record position.
 *
 * @param iter Iterator.
 * @return Position, for settings_raw_iter_set_current_record_pos().
 */
int settings_raw_iter_get_current_record_pos(SettingsRawIter *iter);

/**
 * @brief Move to a position from settings_raw_iter_get_current_record_pos().
 *
 * @param iter Iterator.
 * @param pos Record position.
 */
void settings_raw_iter_set_current_record_pos(SettingsRawIter *iter, int pos);

/**
 * @brief Get the position the current search started from.
 *
 * @param iter Iterator.
 * @return Record position.
 */
int settings_raw_iter_get_resumed_record_pos(SettingsRawIter *iter);

/**
 * @brief Read the key of the current record.
 *
 * @param iter Iterator.
 * @param[out] key Buffer of at least the key length.
 */
void settings_raw_iter_read_key(SettingsRawIter *iter, uint8_t *key);

/**
 * @brief Read the value of the current record.
 *
 * @param iter Iterator.
 * @param[out] val Value buffer.
 * @param val_len Bytes to read, at most the value length.
 */
void settings_raw_iter_read_val(SettingsRawIter *iter, uint8_t *val, int val_len);

/**
 * @brief Read the key and value of the current record in one PFS call.
 *
 * @param iter Iterator.
 * @param[out] key_val_out Buffer of at least key length plus value length bytes; receives the key
 * followed by the value.
 */
void settings_raw_iter_read_key_val(SettingsRawIter *iter, uint8_t *key_val_out);

/**
 * @brief Write the header of the current record.
 *
 * @param iter Iterator.
 * @param hdr Header to write; becomes the current header.
 */
void settings_raw_iter_write_header(SettingsRawIter *iter, SettingsRecordHeader *hdr);

/**
 * @brief Write the key of the current record, using the length from its header.
 *
 * @param iter Iterator.
 * @param key Key.
 */
void settings_raw_iter_write_key(SettingsRawIter *iter, const uint8_t *key);

/**
 * @brief Write the value of the current record, using the length from its header.
 *
 * @param iter Iterator.
 * @param val Value.
 */
void settings_raw_iter_write_val(SettingsRawIter *iter, const uint8_t *val);

/**
 * @brief Write the key and value of the current record in one PFS call.
 *
 * @param iter Iterator.
 * @param key_val Key followed by value, as read by settings_raw_iter_read_key_val().
 */
void settings_raw_iter_write_key_val(SettingsRawIter *iter, const uint8_t *key_val);

/**
 * @brief Write one byte of the value of the current record in place.
 *
 * @param iter Iterator.
 * @param offset Offset within the value.
 * @param byte Byte to write.
 */
void settings_raw_iter_write_byte(SettingsRawIter *iter, int offset, uint8_t byte);

/**
 * @brief Close the underlying file.
 *
 * @param iter Iterator.
 */
void settings_raw_iter_deinit(SettingsRawIter *iter);

/** @} */
