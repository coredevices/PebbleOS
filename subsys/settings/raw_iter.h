/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/settings/settings.h>

/*
 * On-flash format of settings files and a cursor over their records.
 *
 * A settings file is a @ref pbl_settings_file_hdr followed by records, each a record header, the
 * key and the value. Unwritten flash (all ones) marks the end. The record header holds a 32-bit
 * time in version 1 files and a 64-bit one in version 2 files.
 * Record flags are active low: a flag is set by clearing its bit, so it can be written in place.
 *
 * Any PFS error during iteration is treated as a fatal logic error: the file is removed and the
 * system reboots, so that the corruption cannot cause a reboot loop.
 */

/** @brief Magic at the start of every settings file. */
#define SETTINGS_FILE_MAGIC "set"
/** @brief Latest settings file format version, with 64-bit record times. */
#define SETTINGS_FILE_VERSION 2
/** @brief Settings file format version with 32-bit record times. */
#define SETTINGS_FILE_VERSION_TIME32 1

static_assert(sizeof((struct pbl_settings_file_hdr){}.magic) == sizeof(SETTINGS_FILE_MAGIC),
              "The magic has been broken!");

/** @brief Record header, key and value are completely written. */
#define SETTINGS_FLAG_WRITE_COMPLETE (1 << 0)
/** @brief A newer record for the same key is being written. */
#define SETTINGS_FLAG_OVERWRITE_STARTED (1 << 1)
/** @brief A newer record for the same key is complete; this one is dead. */
#define SETTINGS_FLAG_OVERWRITE_COMPLETE (1 << 2)
/** @brief Record is in sync with the phone. */
#define SETTINGS_FLAG_SYNCED (1 << 3)

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
static_assert(PBL_SETTINGS_KEY_MAX_LEN == (1 << KEY_LEN_BITS) - 1);
static_assert(PBL_SETTINGS_VAL_MAX_LEN == SETTINGS_EOF_MARKER - 1);

// Record header of version 1 files
struct PBL_PACKED pbl_settings__record_hdr_time32 {
  uint32_t last_modified;
  uint8_t key_hash;
  uint8_t flags : FLAGS_BITS;
  unsigned int key_len : KEY_LEN_BITS;
  unsigned int val_len : VAL_LEN_BITS;
};

// Record header of version 2 files
struct PBL_PACKED pbl_settings__record_hdr_time64 {
  int64_t last_modified;
  uint8_t key_hash;
  uint8_t flags : FLAGS_BITS;
  unsigned int key_len : KEY_LEN_BITS;
  unsigned int val_len : VAL_LEN_BITS;
};

// Whether a record time can be stored in a version 1 file. The all-ones time is reserved for the
// end marker.
static inline bool pbl_settings__time_fits_time32(int64_t t) {
  return t >= 0 && t < UINT32_MAX;
}

// Size of a record header in the file, which depends on its version.
int pbl_settings__raw_iter_hdr_size(const struct pbl_settings_raw_iter *iter);

/**
 * @brief Initialize an iterator and read the file header.
 *
 * @param[out] iter Iterator.
 * @param file Open file, owned by the iterator afterwards.
 * @param file_name File name, for diagnostics. Must outlive the iterator.
 */
void pbl_settings__raw_iter_init(struct pbl_settings_raw_iter *iter, struct pbl_fs_file *file,
                                 const char *file_name);

/**
 * @brief Write the file header, for newly created files.
 *
 * @param iter Iterator.
 * @param file_hdr Header to write.
 */
void pbl_settings__raw_iter_write_file_header(struct pbl_settings_raw_iter *iter,
                                              struct pbl_settings_file_hdr *file_hdr);

/**
 * @brief Move to the first record.
 *
 * @param iter Iterator.
 */
void pbl_settings__raw_iter_begin(struct pbl_settings_raw_iter *iter);

/**
 * @brief Start a search from the current record.
 *
 * @param iter Iterator.
 */
void pbl_settings__raw_iter_resume(struct pbl_settings_raw_iter *iter);

/**
 * @brief Move to the next record.
 *
 * @param iter Iterator.
 */
void pbl_settings__raw_iter_next(struct pbl_settings_raw_iter *iter);

/**
 * @brief Check whether the iterator is past the last record.
 *
 * @param iter Iterator.
 * @return true at the end of the records.
 */
bool pbl_settings__raw_iter_end(struct pbl_settings_raw_iter *iter);

/**
 * @brief Get the current record position.
 *
 * @param iter Iterator.
 * @return Position, for pbl_settings__raw_iter_set_current_record_pos().
 */
int pbl_settings__raw_iter_get_current_record_pos(struct pbl_settings_raw_iter *iter);

/**
 * @brief Move to a position from pbl_settings__raw_iter_get_current_record_pos().
 *
 * @param iter Iterator.
 * @param pos Record position.
 */
void pbl_settings__raw_iter_set_current_record_pos(struct pbl_settings_raw_iter *iter, int pos);

/**
 * @brief Get the position the current search started from.
 *
 * @param iter Iterator.
 * @return Record position.
 */
int pbl_settings__raw_iter_get_resumed_record_pos(struct pbl_settings_raw_iter *iter);

/**
 * @brief Read the key of the current record.
 *
 * @param iter Iterator.
 * @param[out] key Buffer of at least the key length.
 */
void pbl_settings__raw_iter_read_key(struct pbl_settings_raw_iter *iter, uint8_t *key);

/**
 * @brief Read the value of the current record.
 *
 * @param iter Iterator.
 * @param[out] val Value buffer.
 * @param val_len Bytes to read, at most the value length.
 */
void pbl_settings__raw_iter_read_val(struct pbl_settings_raw_iter *iter, uint8_t *val, int val_len);

/**
 * @brief Read the key and value of the current record in one PFS call.
 *
 * @param iter Iterator.
 * @param[out] key_val_out Buffer of at least key length plus value length bytes; receives the key
 * followed by the value.
 */
void pbl_settings__raw_iter_read_key_val(struct pbl_settings_raw_iter *iter, uint8_t *key_val_out);

/**
 * @brief Write the header of the current record.
 *
 * @param iter Iterator.
 * @param hdr Header to write; becomes the current header.
 */
void pbl_settings__raw_iter_write_header(struct pbl_settings_raw_iter *iter,
                                         struct pbl_settings_record_hdr *hdr);

/**
 * @brief Write the key of the current record, using the length from its header.
 *
 * @param iter Iterator.
 * @param key Key.
 */
void pbl_settings__raw_iter_write_key(struct pbl_settings_raw_iter *iter, const uint8_t *key);

/**
 * @brief Write the value of the current record, using the length from its header.
 *
 * @param iter Iterator.
 * @param val Value.
 */
void pbl_settings__raw_iter_write_val(struct pbl_settings_raw_iter *iter, const uint8_t *val);

/**
 * @brief Write the key and value of the current record in one PFS call.
 *
 * @param iter Iterator.
 * @param key_val Key followed by value, as read by pbl_settings__raw_iter_read_key_val().
 */
void pbl_settings__raw_iter_write_key_val(struct pbl_settings_raw_iter *iter,
                                          const uint8_t *key_val);

/**
 * @brief Write one byte of the value of the current record in place.
 *
 * @param iter Iterator.
 * @param offset Offset within the value.
 * @param byte Byte to write.
 */
void pbl_settings__raw_iter_write_byte(struct pbl_settings_raw_iter *iter, int offset,
                                       uint8_t byte);

/**
 * @brief Close the underlying file.
 *
 * @param iter Iterator.
 */
void pbl_settings__raw_iter_deinit(struct pbl_settings_raw_iter *iter);
