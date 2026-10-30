/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/persist.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/util/list.h>
#include <pbl/util/math.h>
#include <pbl/util/units.h>

#include <kernel/pbl_malloc.h>
#include <process_management/app_install_manager.h>
#include <system/status_errno.h>

PBL_LOG_MODULE_DEFINE(service_persist, CONFIG_SERVICE_PERSIST_LOG_LEVEL);

#define PERSIST_STORAGE_MAX_SPACE     PBL_MIB(1)
#define PERSIST_STORAGE_INITIAL_ALLOC PBL_KIB(4)

typedef struct PersistStore {
  ListNode list_node;
  Uuid uuid;
  SettingsFile file;
  bool file_open;
  uint8_t usage_count; //!< How many clients are using this store
} PersistStore;

// Each open client has a PersistStore structure linked into this list. If both
// a worker and foreground app of the same UUID are running, then they share the
// same store.
static ListNode *s_client_stores;
static PBL_MUTEX_DEFINE(s_mutex);

static bool prv_uuid_list_filter(ListNode *node, void *data) {
  const Uuid *uuid = data;
  PersistStore *store = (PersistStore *)node;
  return uuid_equal(&store->uuid, uuid);
}

static PersistStore *prv_find_open_store(const Uuid *uuid) {
  return (PersistStore *)list_find(s_client_stores, prv_uuid_list_filter, (void *)uuid);
}

static PBL_ALWAYS_INLINE void prv_lock(void) {
  pbl_mutex_lock_lr(&s_mutex, PBL_FOREVER, (uintptr_t)PBL_RETURN_ADDRESS(0));
}

static inline void prv_unlock(void) {
  pbl_mutex_unlock(&s_mutex);
}

// "ps" prefix + 32 hex chars (16-byte UUID) + NUL.
#define PERSIST_FILE_NAME_MAX_LENGTH sizeof("/ps000102030405060708090a0b0c0d0e0f")

static status_t prv_get_file_name(char *name, size_t buf_len, const Uuid *uuid) {
  // Persist files are named "ps<uuid-hex>". The "ps" prefix indicates the file
  // is in SettingsFile format. The UUID is stable across reinstalls, so the
  // file follows the app regardless of its (volatile) AppInstallId.
  const uint8_t *b = (const uint8_t *)uuid;
  return snprintf(name, buf_len,
                  "/ps%02x%02x%02x%02x%02x%02x%02x%02x"
                  "%02x%02x%02x%02x%02x%02x%02x%02x",
                  b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12],
                  b[13], b[14], b[15]);
}

status_t persist_service_delete_file(const Uuid *uuid) {
  char name[PERSIST_FILE_NAME_MAX_LENGTH];

  status_t status = prv_get_file_name(name, sizeof(name), uuid);
  if (FAILED(status)) {
    return status;
  }
  return status_from_errno(pbl_fs_unlink(name));
}

size_t persist_service_get_max_size(void) {
  return PERSIST_STORAGE_MAX_SPACE;
}

// Persist files used to be named "ps%06d", where the id was allocated by the
// legacy persist_map (the "pmap" file), a UUID->id table. They are now named
// "ps<uuid-hex>" directly. The following migrates existing files to the new
// scheme and removes the now-unused pmap.
// TODO: remove this migration once all devices have upgraded.

#define LEGACY_PMAP_FILE_NAME               "/pmap"
#define LEGACY_PERSIST_FILE_NAME_MAX_LENGTH sizeof("/ps000001")
#define LEGACY_PMAP_EOF_ID                  ((int)(~0))

typedef struct PBL_PACKED {
  uint16_t version;
} LegacyPersistMapHeader;

typedef struct PBL_PACKED {
  int id;
  Uuid uuid;
} LegacyPersistMapField;

static status_t prv_copy_file(const char *from, const char *to) {
  struct pbl_fs_file from_file;
  int ret = pbl_fs_open(&from_file, from, PBL_FS_O_READ);
  if (ret < 0) {
    return status_from_errno(ret);
  }

  size_t size = pbl_fs_size(&from_file);
  struct pbl_fs_file to_file;
  ret = pbl_fs_open(&to_file, to, PBL_FS_O_WRITE | PBL_FS_O_CREATE);
  if (ret == 0) {
    ret = (pbl_fs_size(&to_file) == 0) ? pbl_fs_truncate(&to_file, size) : -EEXIST;
    if (ret < 0) {
      pbl_fs_close(&to_file);
    }
  }

  if (ret < 0) {
    pbl_fs_close(&from_file);
    return status_from_errno(ret);
  }

  status_t rv = S_SUCCESS;
  const size_t chunk_size = 128;
  uint8_t *buf = kernel_malloc(chunk_size);
  if (buf == nullptr) {
    rv = E_OUT_OF_MEMORY;
  } else {
    size_t remaining = size;
    while (remaining > 0) {
      size_t n = MIN(remaining, chunk_size);
      if ((ret = pbl_fs_read(&from_file, buf, n)) != (int)n) {
        rv = (ret >= 0) ? E_INTERNAL : status_from_errno(ret);
        break;
      }
      if ((ret = pbl_fs_write(&to_file, buf, n)) != (int)n) {
        rv = (ret >= 0) ? E_INTERNAL : status_from_errno(ret);
        break;
      }
      remaining -= n;
    }
    kernel_free(buf);
  }

  pbl_fs_close(&from_file);
  pbl_fs_close(&to_file);
  return rv;
}

static void prv_migrate_legacy_persist_files(void) {
  struct pbl_fs_file file;
  if (pbl_fs_open(&file, LEGACY_PMAP_FILE_NAME, PBL_FS_O_READ) < 0) {
    // No legacy map, nothing to migrate.
    return;
  }

  pbl_fs_seek(&file, sizeof(LegacyPersistMapHeader), PBL_FS_SEEK_SET);

  LegacyPersistMapField field;
  while (pbl_fs_read(&file, (uint8_t *)&field, sizeof(field)) == (int)sizeof(field)) {
    if (field.id == LEGACY_PMAP_EOF_ID) {
      break;
    }

    char old_name[LEGACY_PERSIST_FILE_NAME_MAX_LENGTH];
    snprintf(old_name, sizeof(old_name), "/ps%06d", field.id);

    char new_name[PERSIST_FILE_NAME_MAX_LENGTH];
    if (FAILED(prv_get_file_name(new_name, sizeof(new_name), &field.uuid))) {
      continue;
    }

    if (PASSED(prv_copy_file(old_name, new_name))) {
      pbl_fs_unlink(old_name);
    }
  }

  pbl_fs_close(&file);
  pbl_fs_unlink(LEGACY_PMAP_FILE_NAME);
}

// Designed to be called once during reset
void persist_service_init(void) {
  prv_migrate_legacy_persist_files();
}

// Return a pointer to the store for the given UUID. Each task that uses persist
// must call persist_service_client_open() to create/open the store during its
// startup and persist_service_client_close() during its shutdown.
//
// The SettingsFile is opened/created lazily. A persist file will not be
// created for an app unless it calls a persist function.
//
// The persist service mutex is locked when this function is called. It will
// only be unlocked after a call to persist_service_unlock(). While the global
// persist service mutex is currently used, the API is designed such that a
// per-file mutex could be used without altering the callers.
SettingsFile *persist_service_lock_and_get_store(const Uuid *uuid) {
  prv_lock();
  PersistStore *store = prv_find_open_store(uuid);
  PBL_ASSERTN(store);
  if (!store->file_open) {
    char filename[PERSIST_FILE_NAME_MAX_LENGTH];
    PBL_ASSERTN(PASSED(prv_get_file_name(filename, sizeof(filename), uuid)));
    PBL_ASSERTN(PASSED(settings_file_open_growable(
        &store->file, filename, PERSIST_STORAGE_MAX_SPACE, PERSIST_STORAGE_INITIAL_ALLOC)));
    store->file_open = true;
  }
  return &store->file;
}

void persist_service_unlock_store(SettingsFile *store) {
  prv_unlock();
}

// Create a store for a client of the given UUID it doesn't already exist. If it
// exists already (another client with the same UUID is running), then just
// increment its usage count. This is called by the process startup code
// (app_state_init() or worker_state_init()).
void persist_service_client_open(const Uuid *uuid) {
  prv_lock();
  {
    PersistStore *store = prv_find_open_store(uuid);
    if (store) {
      store->usage_count++;
    } else {
      store = kernel_malloc_check(sizeof(*store));
      *store = (PersistStore){
        .uuid = *uuid,
        .usage_count = 1,
        .file_open = false,
      };
      s_client_stores = list_insert_before(s_client_stores, &store->list_node);
    }
  }
  prv_unlock();
}

// Release the store for the given UUID. Called by ProcessManager to clean up
// after a task exists. If there are no other processes using the same store, it
// will be freed
void persist_service_client_close(const Uuid *uuid) {
  prv_lock();
  {
    PersistStore *store = prv_find_open_store(uuid);
    PBL_ASSERTN(store && list_contains(s_client_stores, &store->list_node) &&
                store->usage_count >= 1);

    if (--store->usage_count == 0) {
      if (store->file_open) {
        settings_file_close(&store->file);
      }
      list_remove(&store->list_node, &s_client_stores /* &head */, nullptr /* &tail */);
      kernel_free(store);
    }
  }
  prv_unlock();
}
