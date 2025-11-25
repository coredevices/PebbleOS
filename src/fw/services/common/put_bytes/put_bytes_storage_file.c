/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "put_bytes_storage_file.h"

#include "services/normal/filesystem/pfs.h"
#include "system/passert.h"

bool pb_storage_file_init(PutBytesStorage *storage, PutBytesObjectType object_type,
                          uint32_t total_size, PutBytesStorageInfo *info, uint32_t append_offset) {
  // if a file exists with that name, remove it
  pfs_remove(info->filename);
  if (total_size == 0) {
    // a file of size zero is valid at the moment
    return true;
  }

  int fd = pfs_open(info->filename, OP_FLAG_READ | OP_FLAG_WRITE, FILE_TYPE_STATIC, total_size);
  storage->impl_data = (void*)(uintptr_t) fd;

  return fd >= 0;
}

uint32_t pb_storage_file_get_max_size(PutBytesObjectType object_type) {
  return get_available_pfs_space();
}

void pb_storage_file_write(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer,
                          uint32_t length) {
  // We don't support writing to arbitrary offsets in this implementation.
  PBL_ASSERTN(offset == storage->current_offset);

  int fd = (int) storage->impl_data;
  pfs_write(fd, buffer, length);
}

uint32_t pb_storage_file_calculate_crc(PutBytesStorage *storage, PutBytesCrcType crc_type) {
  PBL_ASSERTN(crc_type == PutBytesCrcType_Legacy); // PFS doesn't use new checksum at the moment

  int fd = (int) storage->impl_data;
  return pfs_crc_calculate_file(fd, 0, storage->current_offset);
}

void pb_storage_file_deinit(PutBytesStorage *storage, bool is_success) {
  int fd = (int) storage->impl_data;

  if (!is_success) {
    pfs_close_and_remove(fd);
  } else {
    pfs_close(fd);
  }
}
