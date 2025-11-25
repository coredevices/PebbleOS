/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "get_bytes_storage.h"

#include "kernel/pbl_malloc.h"
#include "system/passert.h"
#include "util/size.h"

//! Skeleton implementation struct
typedef struct GetBytesStorageImplementation {
  GetBytesStorageType type;
  bool (*setup)(GetBytesStorage *storage, GetBytesObjectType object_type,
                GetBytesStorageInfo *info);
  GetBytesInfoErrorCode (*get_size)(GetBytesStorage *storage, uint32_t *size);
  bool (*read_next_chunk)(GetBytesStorage *storage, uint8_t *buffer, uint32_t len);
  void (*cleanup)(GetBytesStorage *storage, bool successful);
} GetBytesStorageImplementation;

//! List of storage implementations and their functions
//! If one is not included in PRF, then it is ifdef'ed out.

#include "get_bytes_storage_coredump.h"
#include "get_bytes_storage_file.h"
#include "get_bytes_storage_flash.h"

static const GetBytesStorageImplementation s_get_bytes_impls[] = {
  {
    // Coredump Storage
    .type = GetBytesStorageTypeCoredump,
    .setup = gb_storage_coredump_setup,
    .get_size = gb_storage_coredump_get_size,
    .read_next_chunk = gb_storage_coredump_read_next_chunk,
    .cleanup = gb_storage_coredump_cleanup
  },
#if !defined(RECOVERY_FW) && !defined(RELEASE)
  {
    // Filesystem File Storage
    .type = GetBytesStorageTypeFile,
    .setup = gb_storage_file_setup,
    .get_size = gb_storage_file_get_size,
    .read_next_chunk = gb_storage_file_read_next_chunk,
    .cleanup = gb_storage_file_cleanup
  },
  {
    // Flash Storage
    .type = GetBytesStorageTypeFlash,
    .setup = gb_storage_flash_setup,
    .get_size = gb_storage_flash_get_size,
    .read_next_chunk = gb_storage_flash_read_next_chunk,
    .cleanup = gb_storage_flash_cleanup
  },
#endif /* !defined(RECOVERY_FW) && !defined(RELEASE) */
};

//! Return the storage type for the object type.
GetBytesStorageType prv_get_storage_type_for_object(GetBytesObjectType object_type) {
  switch (object_type) {
    case GetBytesObjectCoredump:
      return GetBytesStorageTypeCoredump;
    case GetBytesObjectFile:
      return GetBytesStorageTypeFile;
    case GetBytesObjectFlash:
      return GetBytesStorageTypeFlash;
    default:
      WTF;
      return GetBytesStorageTypeUnknown;
  }
}

bool gb_storage_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                     GetBytesStorageInfo *info) {
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_get_bytes_impls); i++) {
    if (s_get_bytes_impls[i].type == prv_get_storage_type_for_object(object_type)) {
      storage->impl = &s_get_bytes_impls[i];

      if (storage->impl->setup) {
        return storage->impl->setup(storage, object_type, info);
      }
    }
  }

  return false;
}

GetBytesInfoErrorCode gb_storage_get_size(GetBytesStorage *storage, uint32_t *size) {
  if (storage->impl->setup) {
    return storage->impl->get_size(storage, size);
  }

  return 0;
}

bool gb_storage_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len) {
  if (storage->impl->setup) {
    return storage->impl->read_next_chunk(storage, buffer, len);
  }

  return false;
}

void gb_storage_cleanup(GetBytesStorage *storage, bool successful) {
  if (storage->impl->setup) {
    storage->impl->cleanup(storage, successful);
  }
}
