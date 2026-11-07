/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/coredump/coredump.h>
#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/get_bytes/get_bytes_private.h>
#include <pbl/services/get_bytes/get_bytes_storage_coredump.h>

typedef struct {
  struct pbl_coredump_stored stored;
  bool only_get_new_coredump;
} GBCoredumpData;

bool gb_storage_coredump_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                               GetBytesStorageInfo *info) {
  storage->impl_data = pbl_zalloc_check(sizeof(GBCoredumpData));
  ((GBCoredumpData *)storage->impl_data)->only_get_new_coredump = info->only_get_new_coredump;
  return true;
}

GetBytesInfoErrorCode gb_storage_coredump_get_size(GetBytesStorage *storage, uint32_t *size) {
  GBCoredumpData *data = storage->impl_data;

  if (pbl_coredump_stored_find(data->only_get_new_coredump, &data->stored) != 0) {
    return GET_BYTES_DOESNT_EXIST;
  }

  return (pbl_coredump_stored_size(&data->stored, size) == 0) ? GET_BYTES_OK : GET_BYTES_CORRUPTED;
}

bool gb_storage_coredump_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len) {
  GBCoredumpData *data = storage->impl_data;

  if (pbl_coredump_stored_read(&data->stored, storage->current_offset, buffer, len) != 0) {
    return false;
  }
  storage->current_offset += len;
  return true;
}

void gb_storage_coredump_cleanup(GetBytesStorage *storage, bool successful) {
  if (successful) {
    GBCoredumpData *data = storage->impl_data;
    (void)pbl_coredump_stored_mark_read(&data->stored);
  }

  pbl_free(storage->impl_data);
}
