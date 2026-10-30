/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource_storage_file.h"

#include "language_pack.h"
#include "resource_storage_impl.h"

#include <errno.h>
#include <stdint.h>

#include <pbl/crc/crc.h>
#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>

#include <resource/resource_ids.auto.h>

extern const FileResourceData g_file_resource_stores[];
extern const uint32_t g_num_file_resource_stores;

static_assert(RESOURCE_ID_LECO_60_BOLD_NUMBERS_AM_PM_EXTENDED - RESOURCE_ID_STRINGS <
                  LANGUAGE_PACK_RESOURCE_STRIDE,
              "language pack resources must fit in a slot");

// Each extra slot owns its whole ID window; the file says which entries it carries.
static const FileResourceData s_language_resource_stores[LANGUAGE_PACK_COUNT - 1] = {
#define LANGUAGE_STORE(slot)                                                   \
  {LANGUAGE_PACK_RESOURCE_BASE + (slot) * LANGUAGE_PACK_RESOURCE_STRIDE + 1,   \
   LANGUAGE_PACK_RESOURCE_BASE + ((slot) + 1) * LANGUAGE_PACK_RESOURCE_STRIDE, \
   LANGUAGE_PACK_RESOURCE_BASE + (slot) * LANGUAGE_PACK_RESOURCE_STRIDE, "/lang" #slot}
  LANGUAGE_STORE(1),
  LANGUAGE_STORE(2),
  LANGUAGE_STORE(3),
#undef LANGUAGE_STORE
};

bool language_pack_is_installed(uint8_t slot) {
  const char *filename = language_pack_filename(slot);
  if (!filename) {
    return false;
  }
  struct pbl_fs_file file;
  const int ret = pbl_fs_open(&file, filename, PBL_FS_O_READ | PBL_FS_O_PFS_SKIP_HDR_CRC_CHECK);
  if (ret == 0) {
    pbl_fs_close(&file);
  }
  return ret != -ENOENT;
}

// Common helpers functions
//
// These functions are highly coupled to the ones that call them but they're just for code
// deduplication and not actually intended to be reusable or provide encapsulation.

static uint32_t prv_file_common_get_length_and_close(struct pbl_fs_file *file, int ret) {
  if (ret < 0) {
    return 0;
  }
  uint32_t length = pbl_fs_size(file);
  pbl_fs_close(file);
  return length;
}

static uint32_t prv_file_common_get_crc(struct pbl_fs_file *file, int ret, uint32_t num_bytes,
                                        uint32_t entry_offset) {
  if (ret < 0) {
    return 0xFFFFFFFF;
  }

  struct pbl_crc32_legacy checksum;
  pbl_crc32_legacy_init(&checksum);

  uint8_t buffer[128];
  pbl_fs_seek(file, RESOURCE_STORE_METADATA_BYTES + entry_offset, PBL_FS_SEEK_SET);
  while (num_bytes > 0) {
    const ssize_t bytes_read = pbl_fs_read(file, buffer, MIN(num_bytes, sizeof(buffer)));
    if (bytes_read <= 0) {
      break;
    }
    pbl_crc32_legacy_update(&checksum, buffer, bytes_read);
    num_bytes -= bytes_read;
  }

  pbl_fs_close(file);
  return pbl_crc32_legacy_finish(&checksum);
}

static uint32_t prv_file_common_read(struct pbl_fs_file *file, int ret, uint32_t offset, void *data,
                                     size_t num_bytes) {
  if (ret < 0) {
    return 0;
  }

  ssize_t bytes_read = 0;

  if (pbl_fs_seek(file, offset, PBL_FS_SEEK_SET) >= 0) {
    bytes_read = pbl_fs_read(file, data, num_bytes);

    // prevent invalid resource API return values, and partial reads past the end
    if (bytes_read != (ssize_t)num_bytes) {
      bytes_read = 0;
    }
  }
  pbl_fs_close(file);

  return bytes_read;
}

///////////////////////////////////////////////////////////////////////////////
// ResourceStoreTypeFile implementation

#define FAST_READ_FLAGS \
  (PBL_FS_O_READ | PBL_FS_O_PFS_SKIP_HDR_CRC_CHECK | PBL_FS_O_PFS_USE_PAGE_CACHE)

static int prv_file_open_by_name(struct pbl_fs_file *file, const char *name, uint32_t flags) {
  int ret = pbl_fs_open(file, name, flags);

  if ((ret < 0) && (ret != -ENOENT)) {
    PBL_LOG_WRN("Could not open resource pfs file <%s>, ret: %d", name, ret);
  }

  return ret;
}

static int prv_file_open(struct pbl_fs_file *file, ResourceStoreEntry *entry, uint32_t flags) {
  return prv_file_open_by_name(file, ((FileResourceData *)entry->store_data)->name, flags);
}

static uint32_t resource_storage_file_get_length(ResourceStoreEntry *entry) {
  struct pbl_fs_file file;
  int ret = prv_file_open(&file, entry, FAST_READ_FLAGS);
  return prv_file_common_get_length_and_close(&file, ret);
}

static uint32_t resource_storage_file_get_crc(ResourceStoreEntry *entry, uint32_t num_bytes,
                                              uint32_t entry_offset) {
  struct pbl_fs_file file;
  int ret = prv_file_open(&file, entry, PBL_FS_O_READ);
  return prv_file_common_get_crc(&file, ret, num_bytes, entry_offset);
}

static uint32_t resource_storage_file_read(ResourceStoreEntry *entry, uint32_t offset, void *data,
                                           size_t num_bytes) {
  struct pbl_fs_file file;
  int ret = prv_file_open(&file, entry, FAST_READ_FLAGS);
  return prv_file_common_read(&file, ret, offset, data, num_bytes);
}

static const uint8_t *resource_storage_file_readonly_bytes_unsupported(ResourceStoreEntry *entry,
                                                                       bool has_privileged_access) {
  return nullptr;
}

static bool resource_storage_file_find_resource(ResourceStoreEntry *entry, ResAppNum app_num,
                                                uint32_t resource_id) {
  if (app_num != SYSTEM_APP) {
    return false;
  }
  for (unsigned int i = 0; i < LANGUAGE_PACK_COUNT - 1; ++i) {
    const FileResourceData *file = &s_language_resource_stores[i];
    if (resource_id >= file->first_resource_id && resource_id <= file->last_resource_id) {
      entry->store_data = file;
      entry->id -= file->resource_id_offset;
      return true;
    }
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
                                                          void *data) {
  const FileResourceData *file = entry->store_data;
  if (!file) {
    return nullptr;
  }
  return pbl_fs_watch_add(file->name, PBL_FS_WATCH_ALL, callback, data);
}

static bool resource_storage_file_unwatch(ResourceCallbackHandle cb_handle) {
  pbl_fs_watch_remove(cb_handle);
  return true;
}

static void resource_storage_file_init(void) {
  // Make sure the files we have are valid
  for (unsigned int i = 0; i < g_num_file_resource_stores; ++i) {
    // The only way we can check this file is valid is by making sure each resource in each file
    // is valid.

    // Get the length of the file to see if we're checking a large file
    const char *name = g_file_resource_stores[i].name;
    struct pbl_fs_file file;
    const int ret = prv_file_open_by_name(&file, name, FAST_READ_FLAGS);

    const uint32_t file_length = prv_file_common_get_length_and_close(&file, ret);

    // Files written for older firmware may carry fewer entries
    const uint32_t first_resource_id = g_file_resource_stores[i].first_resource_id;
    const uint32_t num_entries = resource_storage_get_num_entries(SYSTEM_APP, first_resource_id);
    const uint32_t last_resource_id =
        MIN(g_file_resource_stores[i].last_resource_id, first_resource_id + num_entries - 1);

    // Now check each entry in the file
    for (uint32_t resource_id = first_resource_id; resource_id <= last_resource_id; resource_id++) {
      if (!resource_storage_check(SYSTEM_APP, resource_id, nullptr)) {
        PBL_LOG_ERR("System resource file %" PRIu32 " corrupt!!!", resource_id);
      }

      const uint32_t large_file_size_threshold = 200 * 1024;
      if (file_length > large_file_size_threshold) {
        // If this file is over 200KB, it's going to take a while to CRC. Let's sleep a bit
        // between entries so we don't starve out our background task.
        pbl_sleep_ms(5);
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

static int prv_app_file_open(struct pbl_fs_file *file, ResourceStoreEntry *entry, uint32_t flags) {
  ResAppNum app_num = (ResAppNum)(uintptr_t)entry->store_data;
  if (app_num == SYSTEM_APP) {
    return -EINVAL;
  }
  char filename[APP_RESOURCE_FILENAME_MAX_LENGTH + 1]; // extra for null terminator
  resource_storage_get_file_name(filename, sizeof(filename), app_num);
  return prv_file_open_by_name(file, filename, flags);
}

static bool resource_storage_app_file_find_resource(ResourceStoreEntry *entry, ResAppNum app_num,
                                                    uint32_t resource_id) {
  if (app_num == SYSTEM_APP) {
    return false;
  }
  // Need to cast to uintptr_t first to make test compiling on 64-bit happy
  entry->store_data = (void *)(uintptr_t)app_num;
  return true;
}

static void resource_storage_app_file_clear(ResourceStoreEntry *entry) {
  ResAppNum app_num = (ResAppNum)(uintptr_t)entry->store_data;
  if (app_num == SYSTEM_APP) {
    return;
  }
  char filename[APP_RESOURCE_FILENAME_MAX_LENGTH + 1]; // extra for null terminator
  resource_storage_get_file_name(filename, sizeof(filename), app_num);
  pbl_fs_unlink(filename);
}

static uint32_t resource_storage_app_file_get_length(ResourceStoreEntry *entry) {
  struct pbl_fs_file file;
  int ret = prv_app_file_open(&file, entry, FAST_READ_FLAGS);
  return prv_file_common_get_length_and_close(&file, ret);
}

static uint32_t resource_storage_app_file_get_crc(ResourceStoreEntry *entry, uint32_t num_bytes,
                                                  uint32_t entry_offset) {
  struct pbl_fs_file file;
  int ret = prv_app_file_open(&file, entry, PBL_FS_O_READ);
  return prv_file_common_get_crc(&file, ret, num_bytes, entry_offset);
}

static uint32_t resource_storage_app_file_read(ResourceStoreEntry *entry, uint32_t offset,
                                               void *data, size_t num_bytes) {
  struct pbl_fs_file file;
  int ret = prv_app_file_open(&file, entry, FAST_READ_FLAGS);
  return prv_file_common_read(&file, ret, offset, data, num_bytes);
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
