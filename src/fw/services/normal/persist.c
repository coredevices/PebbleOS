/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kernel/pbl_malloc.h"
#include "os/mutex.h"
#include "process_management/app_install_manager.h"
#include "services/normal/filesystem/app_file.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/legacy/persist_map.h"
#include "services/normal/settings/settings_file.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/list.h"
#include "util/units.h"

#define PERSIST_STORAGE_MAX_SPACE KiBYTES(6)

typedef struct PersistStore {
  ListNode  list_node;
  Uuid uuid;
  SettingsFile file;
  bool file_open;
  uint8_t usage_count;          //!< How many clients are using this store
} PersistStore;

// Each open client has a PersistStore structure linked into this list. If both
// a worker and foreground app of the same UUID are running, then they share the
// same store.
static ListNode *s_client_stores;
static PebbleMutex *s_mutex;


static bool prv_uuid_list_filter(ListNode* node, void* data) {
  const Uuid *uuid = data;
  PersistStore* store = (PersistStore*)node;
  return uuid_equal(&store->uuid, uuid);
}

static PersistStore * prv_find_open_store(const Uuid *uuid) {
    return (PersistStore *)list_find(s_client_stores, prv_uuid_list_filter,
                                     (void *)uuid);
}

static ALWAYS_INLINE void prv_lock(void) {
  mutex_lock_with_lr(s_mutex, (uint32_t)__builtin_return_address(0));
}

static inline void prv_unlock(void) {
  mutex_unlock(s_mutex);
}

#define PERSIST_FILE_NAME_MAX_LENGTH sizeof("ps000001")

static status_t prv_get_file_name(char *name, size_t buf_len, const Uuid *uuid) {
  // Firmware 2.x persist files are named "p%06d", the added "s" in the file
  // name prefix indicates that it is in SettingsFile format.
  int pid = persist_map_auto_id(uuid);
  if (FAILED(pid)) {
    // Attempting to debug persist map failure
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to get pid! %d", pid);
    persist_map_dump();
    return pid;
  }
  return snprintf(name, buf_len, "ps%06d", pid);
}

status_t persist_service_delete_file(const Uuid *uuid) {
  char name[PERSIST_FILE_NAME_MAX_LENGTH];

  status_t status = prv_get_file_name(name, sizeof(name), uuid);
  if (FAILED(status)) {
    return status;
  }
  return pfs_remove(name);
}

static bool prv_bad_persist_file_filter(const char *filename) {
  return is_app_file_name(filename) &&
         strcmp(filename + APP_FILE_NAME_PREFIX_LENGTH, "persist") == 0;
}

// Designed to be called once during reset
void persist_service_init(void) {
  persist_map_init();
  s_mutex = mutex_create();

  // Find and delete any AppInstallId-indexed persist files. Due to PBL-16663
  // (affecting FW 3.0-dp5 thru -dp7), the AppInstallId in the file name may not
  // correspond to the app that the persist file originally belonged to. Since
  // we can't be sure that the persist files correspond to the current
  // AppInstallId, the safest thing to do is to simply blow them away.
  // TODO: remove this code before FW 3.0-golden.
  PFSFileListEntry *bad_file_list = pfs_create_file_list(
      prv_bad_persist_file_filter);
  PFSFileListEntry *iter = bad_file_list;
  while (iter) {
    pfs_remove(iter->name);
    iter = (PFSFileListEntry *)iter->list_node.next;
  }
  pfs_delete_file_list(bad_file_list);
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
SettingsFile * persist_service_lock_and_get_store(const Uuid *uuid) {
  prv_lock();
  PersistStore *store = prv_find_open_store(uuid);
  PBL_ASSERTN(store);
  if (!store->file_open) {
    char filename[PERSIST_FILE_NAME_MAX_LENGTH];
    PBL_ASSERTN(PASSED(prv_get_file_name(filename, sizeof(filename), uuid)));
    PBL_ASSERTN(PASSED(settings_file_open(&store->file, filename, PERSIST_STORAGE_MAX_SPACE)));
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
      *store = (PersistStore) {
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
    PBL_ASSERTN(store &&
                list_contains(s_client_stores, &store->list_node) &&
                store->usage_count >= 1);

    if (--store->usage_count == 0) {
      if (store->file_open) {
        settings_file_close(&store->file);
      }
      list_remove(&store->list_node,
                  &s_client_stores /* &head */, NULL /* &tail */);
      kernel_free(store);
    }
  }
  prv_unlock();
}
