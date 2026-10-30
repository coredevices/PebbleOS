/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/logging/logging.h>
#include <pbl/services/settings/settings_raw_iter.h>
#include <pbl/util/math.h>

#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DECLARE(service_settings, CONFIG_SERVICE_SETTINGS_LOG_LEVEL);

///////////////////////////////////////////////////
// Helper functions for handling internal errors //
///////////////////////////////////////////////////

#if UNITTEST
static uint32_t s_num_record_changes;
#endif

static uint8_t *read_file_into_ram(SettingsRawIter *iter) {
  int pos = pbl_fs_tell(&iter->file);
  int file_size = pbl_fs_size(&iter->file);
  if (file_size < 0) {
    return nullptr;
  }
  int read_size = file_size;
  uint8_t *contents = kernel_calloc(1, read_size);
  while (contents == nullptr && read_size > 0) {
    // If we can't allocate enough RAM to read the whole file, we should
    // at least try to read part of it.
    read_size /= 2;
    contents = kernel_calloc(1, read_size);
  }
  if (contents == nullptr) {
    PBL_LOG_ERR("Could not allocate %d bytes for corrupt file of size %d.", read_size, file_size);
    return nullptr;
  }
  // In case reading the whole file is not possible due to RAM limitations,
  // read the portions nearest the current seek position, as they are most
  // likely to be the culprit.
  int end_offset = MIN(pos + read_size / 2, file_size);
  int start_offset = end_offset - read_size;
  int status = pbl_fs_seek(&iter->file, start_offset, PBL_FS_SEEK_SET);
  if (status < 0) {
    PBL_LOG_ERR("Debug seek failed: %d", status);
    kernel_free(contents);
    return nullptr;
  }
  int actual_read_size = pbl_fs_read(&iter->file, contents, read_size);
  PBL_LOG_INFO("Read %d (expected %d) bytes of file %s (size %d), around offset %d.",
               actual_read_size, read_size, iter->file_name, file_size, pos);
  return contents;
}

[[noreturn]] static void fatal_logic_error(SettingsRawIter *iter) {
  PBL_LOG_ERR(
      "settings_raw_iter logic error. "
      "Attempting to read affected file into RAM for easier debugging...");
  uint8_t *contents = read_file_into_ram(iter);
  PBL_LOG_ERR("Removing affected file %s...", iter->file_name);
  // Remove the file that caused us to get into this state before we reboot,
  // that way we should be able to avoid getting into a reboot loop.
  pbl_fs_close(&iter->file);
  pbl_fs_unlink(iter->file_name);
  PBL_LOG_ERR("Data at address %p. Rebooting...", contents);
  pbl_panic("Internal logic error.");
}

static int sfs_seek(SettingsRawIter *iter, int amount, int whence) {
  int status = pbl_fs_seek(&iter->file, amount, whence);
  if (status >= 0) {
    return status;
  }

  int pos = pbl_fs_tell(&iter->file);
  PBL_LOG_ERR("Could not seek by %d from whence %d at pos %d: %d", amount, whence, pos, status);
  fatal_logic_error(iter);
}

static int sfs_pos(SettingsRawIter *iter) {
  return sfs_seek(iter, 0, PBL_FS_SEEK_CUR);
}

static int sfs_read(SettingsRawIter *iter, uint8_t *data, int data_len) {
  if (data_len == 0) {
    return 0;
  }
  int status = pbl_fs_read(&iter->file, data, data_len);
  if (status == data_len) {
    return status;
  }

  int pos = pbl_fs_tell(&iter->file);
  PBL_LOG_ERR("Could not read data to %p of length %d at pos %d: %d", data, data_len, pos, status);
  fatal_logic_error(iter);
}

static int sfs_write(SettingsRawIter *iter, const uint8_t *data, int data_len) {
  if (data_len == 0) {
    return 0;
  }
  int status = pbl_fs_write(&iter->file, data, data_len);
  if (status == data_len) {
    return status;
  }

  int pos = pbl_fs_tell(&iter->file);
  PBL_LOG_ERR("Could not write from %p, %d bytes at pos %d: %d", data, data_len, pos, status);
  fatal_logic_error(iter);
}

///////////////////////////
// Actual Iteration Code //
///////////////////////////

void settings_raw_iter_init(SettingsRawIter *iter, struct pbl_fs_file *file,
                            const char *file_name) {
  *iter = (SettingsRawIter){};
  iter->file = *file;
  iter->file_name = file_name;

  sfs_seek(iter, 0, PBL_FS_SEEK_SET);
  sfs_read(iter, (uint8_t *)&iter->file_hdr, sizeof(iter->file_hdr));
  iter->hdr_pos = -1;
  iter->resumed_pos = -1;
}
void settings_raw_iter_write_file_header(SettingsRawIter *iter, SettingsFileHeader *file_hdr) {
  sfs_seek(iter, 0, PBL_FS_SEEK_SET);
  sfs_write(iter, (uint8_t *)file_hdr, sizeof(*file_hdr));
  iter->file_hdr = *file_hdr;
  iter->hdr_pos = -1;
  iter->resumed_pos = -1;
}

void settings_raw_iter_begin(SettingsRawIter *iter) {
  sfs_seek(iter, sizeof(iter->file_hdr), PBL_FS_SEEK_SET);

  // Read header for first record.
  iter->hdr_pos = sfs_pos(iter);
  iter->resumed_pos = iter->hdr_pos;
  sfs_read(iter, (uint8_t *)&iter->hdr, sizeof(iter->hdr));
}

void settings_raw_iter_resume(SettingsRawIter *iter) {
  iter->resumed_pos = iter->hdr_pos;
}

void settings_raw_iter_next(SettingsRawIter *iter) {
  // Seek to start of next record header.
  int record_len = sizeof(iter->hdr) + iter->hdr.key_len + iter->hdr.val_len;
  sfs_seek(iter, iter->hdr_pos + record_len, PBL_FS_SEEK_SET);

  // Read the next header
  iter->hdr_pos = sfs_pos(iter);
  sfs_read(iter, (uint8_t *)&iter->hdr, sizeof(iter->hdr));

#if UNITTEST
  s_num_record_changes++;
#endif
}
bool settings_raw_iter_end(SettingsRawIter *iter) {
  SettingsRecordHeader *hdr = &iter->hdr;
  return (hdr->last_modified == 0xffffffff) &&
         (hdr->flags == ((1 << FLAGS_BITS) - 1) && (hdr->key_hash == 0xff) &&
          (hdr->key_len == ((1 << KEY_LEN_BITS) - 1)) && (hdr->val_len == SETTINGS_EOF_MARKER));
}

int settings_raw_iter_get_current_record_pos(SettingsRawIter *iter) {
  return iter->hdr_pos;
}

int settings_raw_iter_get_resumed_record_pos(SettingsRawIter *iter) {
  return iter->resumed_pos;
}

void settings_raw_iter_set_current_record_pos(SettingsRawIter *iter, int pos) {
  sfs_seek(iter, pos, PBL_FS_SEEK_SET);
  iter->hdr_pos = pos;
  sfs_read(iter, (uint8_t *)&iter->hdr, sizeof(iter->hdr));
}

void settings_raw_iter_read_key(SettingsRawIter *iter, uint8_t *key_out) {
  if (iter->hdr.key_len == 0)
    return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader), PBL_FS_SEEK_SET);
  sfs_read(iter, key_out, iter->hdr.key_len);
}
void settings_raw_iter_read_val(SettingsRawIter *iter, uint8_t *val_out, int val_len) {
  if (iter->hdr.val_len == 0)
    return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader) + iter->hdr.key_len, PBL_FS_SEEK_SET);
  sfs_read(iter, val_out, val_len);
}

void settings_raw_iter_read_key_val(SettingsRawIter *iter, uint8_t *key_val_out) {
  const int kv_len = iter->hdr.key_len + iter->hdr.val_len;
  if (kv_len == 0)
    return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader), PBL_FS_SEEK_SET);
  sfs_read(iter, key_val_out, kv_len);
}

void settings_raw_iter_write_header(SettingsRawIter *iter, SettingsRecordHeader *hdr) {
  PBL_ASSERTN(hdr->key_len <= SETTINGS_KEY_MAX_LEN);
  PBL_ASSERTN(hdr->val_len <= SETTINGS_VAL_MAX_LEN);
  sfs_seek(iter, iter->hdr_pos, PBL_FS_SEEK_SET);
  sfs_write(iter, (uint8_t *)hdr, sizeof(*hdr));
  iter->hdr = *hdr;
}
void settings_raw_iter_write_key(SettingsRawIter *iter, const uint8_t *key) {
  if (iter->hdr.key_len == 0)
    return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader), PBL_FS_SEEK_SET);
  sfs_write(iter, key, iter->hdr.key_len);
}
void settings_raw_iter_write_val(SettingsRawIter *iter, const uint8_t *val) {
  if (iter->hdr.val_len == 0)
    return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader) + iter->hdr.key_len, PBL_FS_SEEK_SET);
  sfs_write(iter, val, iter->hdr.val_len);
}

void settings_raw_iter_write_key_val(SettingsRawIter *iter, const uint8_t *key_val) {
  const int kv_len = iter->hdr.key_len + iter->hdr.val_len;
  if (kv_len == 0)
    return;
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader), PBL_FS_SEEK_SET);
  sfs_write(iter, key_val, kv_len);
}
void settings_raw_iter_write_byte(SettingsRawIter *iter, int offset, uint8_t byte) {
  sfs_seek(iter, iter->hdr_pos + sizeof(SettingsRecordHeader) + iter->hdr.key_len + offset,
           PBL_FS_SEEK_SET);
  sfs_write(iter, &byte, 1);
}

void settings_raw_iter_deinit(SettingsRawIter *iter) {
  int status = pbl_fs_close(&iter->file);
  if (status < 0) {
    PBL_LOG_WRN("Could not close settings file");
  }
}

#if UNITTEST
uint32_t settings_raw_iter_prv_get_num_record_searches(void) {
  return s_num_record_changes;
}
#endif
