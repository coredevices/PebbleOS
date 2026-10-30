/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/fs/fs.h>
#include <pbl/services/get_bytes/get_bytes_storage_file.h>

#include <kernel/pbl_malloc.h>

bool gb_storage_file_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                           GetBytesStorageInfo *info) {
  storage->impl_data = nullptr;

  char path[sizeof("/") + PBL_FS_NAME_MAX];
  if (snprintf(path, sizeof(path), "/%s", info->filename) >= (int)sizeof(path)) {
    return false;
  }

  struct pbl_fs_file *file = kernel_malloc_check(sizeof(*file));
  if (pbl_fs_open(file, path, PBL_FS_O_READ) < 0) {
    kernel_free(file);
    return false;
  }

  storage->impl_data = file;
  return true;
}

GetBytesInfoErrorCode gb_storage_file_get_size(GetBytesStorage *storage, uint32_t *size) {
  *size = pbl_fs_size(storage->impl_data);
  return GET_BYTES_OK;
}

bool gb_storage_file_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len) {
  storage->current_offset += len;
  return (pbl_fs_read(storage->impl_data, buffer, len) == (ssize_t)len);
}

void gb_storage_file_cleanup(GetBytesStorage *storage, bool successful) {
  struct pbl_fs_file *file = storage->impl_data;
  if (file != nullptr) {
    pbl_fs_close(file);
    kernel_free(file);
  }
}
