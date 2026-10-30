/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/crc/crc.h>
#include <pbl/fs/fs.h>
#include <pbl/kernel/assert.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/services/put_bytes/put_bytes_storage_file.h>
#include <pbl/util/math.h>

#include <kernel/pbl_malloc.h>

typedef struct {
  struct pbl_fs_file file;
  char path[];
} PutBytesFile;

bool pb_storage_file_init(PutBytesStorage *storage, PutBytesObjectType object_type,
                          uint32_t total_size, PutBytesStorageInfo *info, uint32_t append_offset) {
  // Nothing to close in deinit if init bails out before opening the file
  storage->impl_data = nullptr;
  const char *filename = i18n_pack_install_begin(info->filename);
  if (!filename) {
    return false;
  }
  if (filename != info->filename) {
    // Pack file names are never longer than the add request's
    strcpy(info->filename, filename);
  }
  // if a file exists with that name, remove it
  pbl_fs_unlink(info->filename);
  if (total_size == 0) {
    // a file of size zero is valid at the moment
    return true;
  }

  PutBytesFile *pb_file = kernel_malloc_check(sizeof(*pb_file) + strlen(info->filename) + 1);
  strcpy(pb_file->path, info->filename);

  int ret = pbl_fs_open(&pb_file->file, pb_file->path, PBL_FS_O_RDWR | PBL_FS_O_CREATE);
  if (ret == 0) {
    ret = pbl_fs_truncate(&pb_file->file, total_size);
    if (ret < 0) {
      pbl_fs_close(&pb_file->file);
    }
  }

  if (ret < 0) {
    kernel_free(pb_file);
    return false;
  }

  storage->impl_data = pb_file;
  return true;
}

uint32_t pb_storage_file_get_max_size(PutBytesObjectType object_type) {
  struct pbl_fs_statvfs stat;
  if (pbl_fs_statvfs("/", &stat) < 0) {
    return 0;
  }

  return stat.f_bfree * stat.f_frsize;
}

void pb_storage_file_write(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer,
                           uint32_t length) {
  // We don't support writing to arbitrary offsets in this implementation.
  PBL_ASSERTN(offset == storage->current_offset);

  PutBytesFile *pb_file = storage->impl_data;
  if (pb_file != nullptr) {
    pbl_fs_write(&pb_file->file, buffer, length);
  }
}

uint32_t pb_storage_file_calculate_crc(PutBytesStorage *storage, PutBytesCrcType crc_type) {
  PBL_ASSERTN(crc_type == PutBytesCrcType_Legacy); // PFS doesn't use new checksum at the moment

  struct pbl_crc32_legacy checksum;
  pbl_crc32_legacy_init(&checksum);

  PutBytesFile *pb_file = storage->impl_data;
  if (pb_file != nullptr) {
    uint8_t buffer[128];
    uint32_t num_bytes = storage->current_offset;
    pbl_fs_seek(&pb_file->file, 0, PBL_FS_SEEK_SET);
    while (num_bytes > 0) {
      const ssize_t bytes_read =
          pbl_fs_read(&pb_file->file, buffer, MIN(num_bytes, sizeof(buffer)));
      if (bytes_read <= 0) {
        break;
      }
      pbl_crc32_legacy_update(&checksum, buffer, bytes_read);
      num_bytes -= bytes_read;
    }
  }

  return pbl_crc32_legacy_finish(&checksum);
}

void pb_storage_file_deinit(PutBytesStorage *storage, bool is_success) {
  PutBytesFile *pb_file = storage->impl_data;

  if (pb_file != nullptr) {
    pbl_fs_close(&pb_file->file);
    if (!is_success) {
      pbl_fs_unlink(pb_file->path);
    }
    kernel_free(pb_file);
    storage->impl_data = nullptr;
  }
  i18n_pack_install_end(is_success);
}
