/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! This file is not intended for consumption by the general firmware, try
//! settings_file_each() in settings_file.h.

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

#include "system/status_codes.h"
#include "util/attributes.h"

#define SETTINGS_FILE_MAGIC "set"
#define SETTINGS_FILE_VERSION 1

typedef struct PACKED {
  uint32_t magic; // = "set"
  uint16_t version;
  uint16_t flags;
} SettingsFileHeader;

_Static_assert(
  sizeof((SettingsFileHeader) {}.magic) == sizeof(SETTINGS_FILE_MAGIC),
  "The magic has been broken!");

#define SETTINGS_FLAG_WRITE_COMPLETE      (1 << 0)
#define SETTINGS_FLAG_OVERWRITE_STARTED   (1 << 1)
#define SETTINGS_FLAG_OVERWRITE_COMPLETE  (1 << 2)
// Indicate that a record is in sync with the phone
#define SETTINGS_FLAG_SYNCED              (1 << 3)

#define SETTINGS_KEY_MAX_LEN 127
#define SETTINGS_VAL_MAX_LEN (SETTINGS_EOF_MARKER - 1) // we reserve the largest value for EOF

#define KEY_LEN_BITS 7
#define VAL_LEN_BITS 11
#define FLAGS_BITS 6

#define SETTINGS_EOF_MARKER ((1 << VAL_LEN_BITS) - 1)

_Static_assert(KEY_LEN_BITS + VAL_LEN_BITS + FLAGS_BITS == 24,
    "The record header bitfields must add up to 24!");

typedef struct PACKED {
  uint32_t     last_modified;
  uint8_t      key_hash;
  uint8_t      flags:FLAGS_BITS;
  unsigned int key_len:KEY_LEN_BITS;
  unsigned int val_len:VAL_LEN_BITS;
} SettingsRecordHeader;

// A SettingsRawIter is just a more convenient interface for the underlying file
// which has two primary utilities.
//  a) It has an exception handling scheme for when logic is bad or files
//     are corrupted, ensuring we can do something, and that we *always*
//     do something when such unexpected conditions occur
//  b) It ensures the upper layers (i.e. settings_file) can never get confused
//     as to their current position within the file, and end up reading data
//     as a header, reading past the end of a key/value, or other nefarious
//     things.
typedef struct {
  const char           *file_name;
  int                  fd;
  SettingsFileHeader   file_hdr;

  // Header for the record we are currently on.
  SettingsRecordHeader hdr;
  // - Offset within the file pointing to the beginning of a `SettingsRecordHeader`
  // - The header it points to is the one where our iterator is.
  // - Used to make sure we can always skip to the next record properly.
  int                  hdr_pos;
  // - Offset within the file pointing to the beginning of a `SettingsRecordHeader`
  // - The header it points to is the one where we began/resumed searching from.
  // - Only gets changed when calling `settings_raw_iter_(being|resume)`
  // - Used to allow wrapping from the end to the beginning when searching
  //   for a specific record.
  int                  resumed_pos;
} SettingsRawIter;

//! Initialize the iterator for use with the given fd.
void settings_raw_iter_init(SettingsRawIter *iter, int fd, const char *file_name);
//! Useful for newly opened files.
void settings_raw_iter_write_file_header(SettingsRawIter *iter, SettingsFileHeader *file_hdr);

//! Begin iteration from the first record
void settings_raw_iter_begin(SettingsRawIter *iter);

//! Resumes iteration from the current record
void settings_raw_iter_resume(SettingsRawIter *iter);

//! Skip to the next record
void settings_raw_iter_next(SettingsRawIter *iter);
//! Returns true if we are at the end of the records.
bool settings_raw_iter_end(SettingsRawIter *iter);

//! Return the current record position, for later restoration by
//! \ref settings_raw_iter_set_current_record_pos.
int settings_raw_iter_get_current_record_pos(SettingsRawIter *iter);

//! Restore a previous record position from
//! \ref setting_iter_get_current_record_pos
void settings_raw_iter_set_current_record_pos(SettingsRawIter *iter, int pos);

//! Return the resumed record position. This was set when we started searching for a record.
int settings_raw_iter_get_resumed_record_pos(SettingsRawIter *iter);

//! Read the key/val for the current record. (The header is read automatically
//! by settings_raw_iter_next() as part of iteration).
void settings_raw_iter_read_key(SettingsRawIter *iter, uint8_t *key);
void settings_raw_iter_read_val(SettingsRawIter *iter, uint8_t *val, int val_len);

//! Write (over top of) the header/key/val for the current record.
void settings_raw_iter_write_header(SettingsRawIter *iter, SettingsRecordHeader *hdr);
void settings_raw_iter_write_key(SettingsRawIter *iter, const uint8_t *key);
void settings_raw_iter_write_val(SettingsRawIter *iter, const uint8_t *val);

//! Write a byte in place for the current record
void settings_raw_iter_write_byte(SettingsRawIter *iter, int offset, uint8_t byte);

//! Close a settings file and stop iteration.
void settings_raw_iter_deinit(SettingsRawIter *iter);
