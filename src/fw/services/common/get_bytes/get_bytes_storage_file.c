/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "get_bytes_storage_file.h"

#include "services/normal/filesystem/pfs.h"

bool gb_storage_file_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                            GetBytesStorageInfo *info) {
  const int fd = pfs_open(info->filename, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  storage->impl_data = (void*)(uintptr_t) fd;
  return fd >= 0;
}

GetBytesInfoErrorCode gb_storage_file_get_size(GetBytesStorage *storage, uint32_t *size) {
  const int fd = (int) storage->impl_data;
  *size = pfs_get_file_size(fd);
  return GET_BYTES_OK;
}

bool gb_storage_file_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len) {
  const int fd = (int) storage->impl_data;
  storage->current_offset += len;
  return (pfs_read(fd, buffer, len) > 0);
}

void gb_storage_file_cleanup(GetBytesStorage *storage, bool successful) {
  const int fd = (int) storage->impl_data;
  pfs_close(fd);
}
