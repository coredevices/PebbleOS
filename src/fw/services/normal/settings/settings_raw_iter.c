/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "settings_raw_iter.h"

#include "kernel/pbl_malloc.h"
#include "services/normal/filesystem/pfs.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

  ///////////////////////////////////////////////////
 // Helper functions for handling internal errors //
///////////////////////////////////////////////////

#if UNITTEST
static uint32_t s_num_record_changes;
#endif

static uint8_t *read_file_into_ram(SettingsRawIter *iter) {
  int pos = pfs_seek(iter->fd, 0, FSeekCur);
  int file_size = pfs_get_file_size(iter->fd);
  if (file_size < 0) {
    return NULL;
  }
  int read_size = file_size;
  uint8_t *contents = kernel_calloc(1, read_size);
  while (contents == NULL && read_size > 0) {
    // If we can't allocate enough RAM to read the whole file, we should
    // at least try to read part of it.
    read_size /= 2;
    contents = kernel_calloc(1, read_size);
  }
  if (contents == NULL) {
    PBL_LOG(LOG_LEVEL_ERROR,
            "Could not allocate %d bytes for corrupt file of size %d.",
            read_size, file_size);
    return NULL;
  }
  // In case reading the whole file is not possible due to RAM limitations,
  // read the portions nearest the current seek position, as they are most
  // likely to be the culprit.
  int end_offset = MIN(pos + read_size/2, file_size);
  int start_offset = end_offset - read_size;
  int status = pfs_seek(iter->fd, start_offset, FSeekSet);
  if (status < 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "Debug seek failed: %d", status);
    kernel_free(contents);
    return NULL;
  }
  int actual_read_size = pfs_read(iter->fd, contents, read_size);
  PBL_LOG(LOG_LEVEL_INFO,
          "Read %d (expected %d) bytes of file %s (size %d), around offset %d.",
          actual_read_size, read_size, iter->file_name, file_size, pos);
  return contents;
}

static NORETURN fatal_logic_error(SettingsRawIter *iter) {
  PBL_LOG(LOG_LEVEL_ERROR,
          "settings_raw_iter logic error. "
          "Attempting to read affected file into RAM for easier debugging...");
  uint8_t *contents = read_file_into_ram(iter);
  PBL_LOG(LOG_LEVEL_INFO, "Removing affected file %s...", iter->file_name);
  // Remove the file that caused us to get into this state before we reboot,
  // that way we should be able to avoid getting into a reboot loop.
  pfs_close_and_remove(iter->fd);
  PBL_LOG(LOG_LEVEL_INFO, "Data at address %p. Rebooting...", contents);
  PBL_CROAK("Internal logic error.");
}

static int sfs_seek(SettingsRawIter *iter, int amount, int whence) {
  status_t status = pfs_seek(iter->fd, amount, whence);
  if (status >= 0) {
    return status;
  }

  int pos = pfs_seek(iter->fd, 0, FSeekCur);
  PBL_LOG(LOG_LEVEL_ERROR, "Could not seek by %d from whence %d at pos %d: %"PRId32,
          amount, whence, pos, status);
  fatal_logic_error(iter);
}

static int sfs_pos(SettingsRawIter *iter) {
  return sfs_seek(iter, 0, FSeekCur);
}

static int sfs_read(SettingsRawIter *iter, uint8_t *data, int data_len) {
  status_t status = pfs_read(iter->fd, data, data_len);
  if (status >= 0) {
    return status;
  }

  int pos = pfs_seek(iter->fd, 0, FSeekCur);
  PBL_LOG(LOG_LEVEL_ERROR, "Could not read data to %p of length %d at pos %d: %"PRId32,
          data, data_len, pos, status);
  fatal_logic_error(iter);
}

static int sfs_write(SettingsRawIter *iter, const uint8_t *data, int data_len) {
  status_t status = pfs_write(iter->fd, data, data_len);
  if (status >= 0) {
    return status;
  }

  int pos = pfs_seek(iter->fd, 0, FSeekCur);
  PBL_LOG(LOG_LEVEL_ERROR, "Could not write from %p, %d bytes at pos %d: %"PRId32,
          data, data_len, pos, status);
  fatal_logic_error(iter);
}

  ///////////////////////////
 // Actual Iteration Code //
///////////////////////////

void settings_raw_iter_init(SettingsRawIter *iter, int fd, const char *file_name) {
  *iter = (SettingsRawIter){};
  iter->fd = fd;
  iter->file_name = file_name;

  sfs_seek(iter, 0, FSeekSet);
  sfs_read(iter, (uint8_t*)&iter->file_hdr, sizeof(iter->file_hdr));
  iter->hdr_pos = -1;
  iter->resumed_pos = -1;
}
void settings_raw_iter_write_file_header(SettingsRawIter *iter, SettingsFileHeader *file_hdr) {
  sfs_seek(iter, 0, FSeekSet);
  sfs_write(iter, (uint8_t*)file_hdr, sizeof(*file_hdr));
  iter->file_hdr = *file_hdr;
  iter->hdr_pos = -1;
  iter->resumed_pos = -1;
}

void settings_raw_iter_begin(SettingsRawIter *iter) {
  sfs_seek(iter, sizeof(iter->file_hdr), FSeekSet);

  // Read header for first record.
  iter->hdr_pos = sfs_pos(iter);
  iter->resumed_pos = iter->hdr_pos;
  sfs_read(iter, (uint8_t*)&iter->hdr, sizeof(iter->hdr));
}

void settings_raw_iter_resume(SettingsRawIter *iter) {
  iter->resumed_pos = iter->hdr_pos;
}

void settings_raw_iter_next(SettingsRawIter *iter) {
  // Seek to start of next record header.
  int record_len = sizeof(iter->hdr) + iter->hdr.key_len + iter->hdr.val_len;
  sfs_seek(iter, iter->hdr_pos + record_len, FSeekSet);

  // Read the next header
  iter->hdr_pos = sfs_pos(iter);
  sfs_read(iter, (uint8_t*)&iter->hdr, sizeof(iter->hdr));

#if UNITTEST
  s_num_record_changes++;
#endif
}
bool settings_raw_iter_end(SettingsRawIter *iter) {
  SettingsRecordHeader *hdr = &iter->hdr;
  return (hdr->last_modified == 0xffffffff) && (hdr->flags == ((1 << FLAGS_BITS) - 1) &&
      (hdr->key_hash == 0xff) && (hdr->key_len == ((1 << KEY_LEN_BITS) - 1)) &&
      (hdr->val_len == SETTINGS_EOF_MARKER));
}

int settings_raw_iter_get_current_record_pos(SettingsRawIter *iter) {
  return iter->hdr_pos;
}

int settings_raw_iter_get_resumed_record_pos(SettingsRawIter *iter) {
  return iter->resumed_pos;
}

void settings_raw_iter_set_current_record_pos(SettingsRawIter *iter, int pos) {
  sfs_seek(iter, pos, FSeekSet);
  iter->hdr_pos = pos;
  sfs_read(iter, (uint8_t*)&iter->hdr, sizeof(iter->hdr));
}

void settings_raw_iter_read_key(SettingsRawIter *iter, uint8_t *key_out) {
  if (iter->hdr.key_len == 0) return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader), FSeekSet);
  sfs_read(iter, key_out, iter->hdr.key_len);
}
void settings_raw_iter_read_val(SettingsRawIter *iter, uint8_t *val_out, int val_len) {
  if (iter->hdr.val_len == 0) return;
  sfs_seek(iter, iter->hdr_pos
      + sizeof(SettingsRecordHeader) + iter->hdr.key_len, FSeekSet);
  sfs_read(iter, val_out, val_len);
}

void settings_raw_iter_write_header(SettingsRawIter *iter, SettingsRecordHeader *hdr) {
  PBL_ASSERTN(hdr->key_len <= SETTINGS_KEY_MAX_LEN);
  PBL_ASSERTN(hdr->val_len <= SETTINGS_VAL_MAX_LEN);
  sfs_seek(iter, iter->hdr_pos, FSeekSet);
  sfs_write(iter, (uint8_t*)hdr, sizeof(*hdr));
  iter->hdr = *hdr;
}
void settings_raw_iter_write_key(SettingsRawIter *iter, const uint8_t *key) {
  if (iter->hdr.key_len == 0) return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader), FSeekSet);
  sfs_write(iter, key, iter->hdr.key_len);
}
void settings_raw_iter_write_val(SettingsRawIter *iter, const uint8_t *val) {
  if (iter->hdr.val_len == 0) return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader) + iter->hdr.key_len, FSeekSet);
  sfs_write(iter, val, iter->hdr.val_len);
}
void settings_raw_iter_write_byte(SettingsRawIter *iter, int offset, uint8_t byte) {
  sfs_seek(iter, iter->hdr_pos +
      sizeof(SettingsRecordHeader) + iter->hdr.key_len + offset, FSeekSet);
  sfs_write(iter, &byte, 1);
}

void settings_raw_iter_deinit(SettingsRawIter *iter) {
  int status = pfs_close(iter->fd);
  if (status < 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "Could not close settings file");
  }
}

#if UNITTEST
uint32_t settings_raw_iter_prv_get_num_record_searches(void) {
  return s_num_record_changes;
}
#endif
