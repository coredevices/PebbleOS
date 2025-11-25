/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource_storage_impl.h"
#include "resource_storage_file.h"

#include "kernel/util/sleep.h"
#include "services/normal/filesystem/pfs.h"
#include "system/logging.h"

#include <stdint.h>

extern const FileResourceData g_file_resource_stores[];
extern const uint32_t g_num_file_resource_stores;

// Common helpers functions
//
// These functions are highly coupled to the ones that call them but they're just for code
// deduplication and not actually intended to be reusable or provide encapsulation.

static uint32_t prv_file_common_get_length_and_close(int fd) {
  if (fd < 0) {
    return 0;
  }
  uint32_t length = pfs_get_file_size(fd);
  pfs_close(fd);
  return length;
}

static uint32_t prv_file_common_get_crc(int fd, uint32_t num_bytes, uint32_t entry_offset) {
  if (fd < 0) {
    return 0xFFFFFFFF;
  }
  uint32_t crc = pfs_crc_calculate_file(fd, RESOURCE_STORE_METADATA_BYTES + entry_offset,
                                        num_bytes);
  pfs_close(fd);
  return crc;
}

static uint32_t prv_file_common_read(int fd, uint32_t offset, void *data, size_t num_bytes) {
  if (fd < 0) {
    return 0;
  }

  int bytes_read = 0;

  if (pfs_seek(fd, offset, FSeekSet) >= 0) {
    bytes_read = pfs_read(fd, data, num_bytes);

    // prevent invalid resource API return values
    if (bytes_read < 0) {
      bytes_read = 0;
    }
  }
  pfs_close(fd);

  return bytes_read;
}

///////////////////////////////////////////////////////////////////////////////
// ResourceStoreTypeFile implementation

static int prv_file_open_by_name(const char *name, uint8_t op_flags) {
  int fd = pfs_open(name, op_flags, FILE_TYPE_STATIC, 0);

  if ((fd < 0) && (fd != E_DOES_NOT_EXIST)) {
    PBL_LOG(LOG_LEVEL_WARNING, "Could not open resource pfs file <%s>, fd: %d", name, fd);
  }

  return fd;
}

static int prv_file_open(ResourceStoreEntry *entry, uint8_t op_flags) {
  return prv_file_open_by_name(((FileResourceData *) entry->store_data)->name, op_flags);
}

static uint32_t resource_storage_file_get_length(ResourceStoreEntry *entry) {
  const uint8_t op_flags = OP_FLAG_READ | OP_FLAG_SKIP_HDR_CRC_CHECK | OP_FLAG_USE_PAGE_CACHE;
  return prv_file_common_get_length_and_close(prv_file_open(entry, op_flags));
}

static uint32_t resource_storage_file_get_crc(ResourceStoreEntry *entry, uint32_t num_bytes,
                                              uint32_t entry_offset) {
  const uint8_t op_flags = OP_FLAG_READ;
  return prv_file_common_get_crc(prv_file_open(entry, op_flags), num_bytes, entry_offset);
}

static uint32_t resource_storage_file_read(ResourceStoreEntry *entry, uint32_t offset, void *data,
                                           size_t num_bytes) {
  const uint8_t op_flags = OP_FLAG_READ | OP_FLAG_SKIP_HDR_CRC_CHECK | OP_FLAG_USE_PAGE_CACHE;
  return prv_file_common_read(prv_file_open(entry, op_flags), offset, data, num_bytes);
}

static const uint8_t *resource_storage_file_readonly_bytes_unsupported(ResourceStoreEntry *entry,
                                                                       bool has_privileged_access) {
  return NULL;
}

static bool resource_storage_file_find_resource(ResourceStoreEntry *entry, ResAppNum app_num,
                                                uint32_t resource_id) {
  if (app_num != SYSTEM_APP) {
    return false;
  }
  for (unsigned int i = 0; i < g_num_file_resource_stores; ++i) {
    if (g_file_resource_stores[i].first_resource_id > resource_id) {
      break;
    } else if (g_file_resource_stores[i].last_resource_id >= resource_id) {
      const FileResourceData *file = &g_file_resource_stores[i];
      entry->store_data = file;
      entry->id -= file->resource_id_offset;
      return true;
    }
  }

  return false;
}

static ResourceCallbackHandle resource_storage_file_watch(ResourceStoreEntry *entry,
                                                          ResourceChangedCallback callback,
                                                          void* data) {
  const FileResourceData *file = entry->store_data;
  if (!file) {
    return NULL;
  }
  PFSCallbackHandle cb_handle = pfs_watch_file(file->name, callback, FILE_CHANGED_EVENT_ALL, data);
  return cb_handle;
}

static bool resource_storage_file_unwatch(ResourceCallbackHandle cb_handle) {
  pfs_unwatch_file(cb_handle);
  return true;
}

static void resource_storage_file_init(void) {
  // Make sure the files we have are valid
  for (unsigned int i = 0; i < g_num_file_resource_stores; ++i) {
    // The only way we can check this file is valid is by making sure each resource in each file
    // is valid.

    // Get the length of the file to see if we're checking a large file
    const char *name = g_file_resource_stores[i].name;
    const uint8_t op_flags = OP_FLAG_READ | OP_FLAG_SKIP_HDR_CRC_CHECK | OP_FLAG_USE_PAGE_CACHE;
    const int fd = prv_file_open_by_name(name, op_flags);

    const uint32_t file_length = prv_file_common_get_length_and_close(fd);
    PBL_LOG(LOG_LEVEL_INFO, "File %s has length %"PRIu32, name, file_length);

    // Now check each entry in the file
    for (uint32_t resource_id = g_file_resource_stores[i].first_resource_id;
         resource_id <= g_file_resource_stores[i].last_resource_id; resource_id++) {
      // TODO PBL-21402
      if (!resource_storage_check(SYSTEM_APP, resource_id, NULL)) {
        PBL_LOG(LOG_LEVEL_ERROR, "System resource file %"PRIu32" corrupt!!!", resource_id);
      }

      const uint32_t large_file_size_threshold = 200 * 1024;
      if (file_length > large_file_size_threshold) {
        // If this file is over 200KB, it's going to take a while to CRC. Let's sleep a bit
        // between entries so we don't starve out our background task. See PBL-24560 for a real
        // long term fix.
        psleep(5);
      }
    }
  }
}

const ResourceStoreImplementation g_file_impl = {
  .type = ResourceStoreTypeFile,

  .init = resource_storage_file_init,
  .clear = resource_storage_generic_clear,
  .check = resource_storage_generic_check,

  .metadata_size = resource_storage_generic_metadata_size,
  .find_resource = resource_storage_file_find_resource,
  .get_resource = resource_storage_generic_get_resource,

  .get_length = resource_storage_file_get_length,
  .get_crc = resource_storage_file_get_crc,
  .write = resource_storage_generic_write,
  .read = resource_storage_file_read,
  .readonly_bytes = resource_storage_file_readonly_bytes_unsupported,

  .watch = resource_storage_file_watch,
  .unwatch = resource_storage_file_unwatch,
};

///////////////////////////////////////////////////////////////////////////////
// ResourceStoreTypeAppFile implementation

static int prv_app_file_open(ResourceStoreEntry *entry, uint8_t op_flags) {
  ResAppNum app_num = (ResAppNum)entry->store_data;
  if (app_num == SYSTEM_APP) {
    return -1;
  }
  char filename[APP_RESOURCE_FILENAME_MAX_LENGTH + 1]; // extra for null terminator
  resource_storage_get_file_name(filename, sizeof(filename), app_num);
  int fd = pfs_open(filename, op_flags, FILE_TYPE_STATIC, 0);

  if ((fd < 0) && (fd != E_DOES_NOT_EXIST)) {
    PBL_LOG(LOG_LEVEL_WARNING, "Could not open resource pfs file <%s>, fd: %d", filename, fd);
  }

  return fd;
}

static bool resource_storage_app_file_find_resource(ResourceStoreEntry *entry, ResAppNum app_num,
                                                    uint32_t resource_id) {
  if (app_num == SYSTEM_APP) {
    return false;
  }
  // Need to cast to uintptr_t first to make test compiling on 64-bit happy
  entry->store_data = (void*)(uintptr_t)app_num;
  return true;
}

static void resource_storage_app_file_clear(ResourceStoreEntry *entry) {
  ResAppNum app_num = (ResAppNum)entry->store_data;
  if (app_num == SYSTEM_APP) {
    return;
  }
  char filename[APP_RESOURCE_FILENAME_MAX_LENGTH + 1]; // extra for null terminator
  resource_storage_get_file_name(filename, sizeof(filename), app_num);
  pfs_remove(filename);
}

static uint32_t resource_storage_app_file_get_length(ResourceStoreEntry *entry) {
  const uint8_t op_flags = OP_FLAG_READ | OP_FLAG_SKIP_HDR_CRC_CHECK | OP_FLAG_USE_PAGE_CACHE;
  return prv_file_common_get_length_and_close(prv_app_file_open(entry, op_flags));
}

static uint32_t resource_storage_app_file_get_crc(ResourceStoreEntry *entry, uint32_t num_bytes,
                                                  uint32_t entry_offset) {
  const uint8_t op_flags = OP_FLAG_READ;
  return prv_file_common_get_crc(prv_app_file_open(entry, op_flags), num_bytes, entry_offset);
}

static uint32_t resource_storage_app_file_read(ResourceStoreEntry *entry, uint32_t offset,
                                               void *data, size_t num_bytes) {
  const uint8_t op_flags = OP_FLAG_READ | OP_FLAG_SKIP_HDR_CRC_CHECK | OP_FLAG_USE_PAGE_CACHE;
  return prv_file_common_read(prv_app_file_open(entry, op_flags), offset, data, num_bytes);
}

const ResourceStoreImplementation g_app_file_impl = {
  .type = ResourceStoreTypeAppFile,

  .init = resource_storage_generic_init,
  .clear = resource_storage_app_file_clear,
  .check = resource_storage_generic_check,

  .metadata_size = resource_storage_generic_metadata_size,
  .find_resource = resource_storage_app_file_find_resource,
  .get_resource = resource_storage_generic_get_resource,

  .get_length = resource_storage_app_file_get_length,
  .get_crc = resource_storage_app_file_get_crc,
  .write = resource_storage_generic_write,
  .read = resource_storage_app_file_read,
  .readonly_bytes = resource_storage_file_readonly_bytes_unsupported,

  .watch = resource_storage_generic_watch,
  .unwatch = resource_storage_generic_unwatch,
};
