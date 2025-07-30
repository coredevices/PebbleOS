/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "resource.h"
#include "resource_storage.h"
#include "resource_storage_builtin.h"
#include "resource_storage_flash.h"

#include "process_management/app_manager.h"
#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "kernel/pbl_malloc.h"
#include "os/mutex.h"
#include "services/normal/process_management/app_storage.h"
#include "system/logging.h"
#include "system/passert.h"

// TODO: this may be replaced once apps become more dynamic

typedef struct {
  ListNode list_node;
  uint32_t id;
  ResourceStoreEntry stored_resource;
} CachedResource;

PebbleRecursiveMutex *s_resource_mutex = NULL;

static CachedResource *s_resource_list = NULL;

static bool prv_resource_filter(ListNode *found_node, void *data) {
  CachedResource *resource = (CachedResource *)found_node;
  uint32_t resource_id = (uint32_t)data;

  return (resource->id == resource_id);
}

static void prv_get_resource(ResAppNum app_num, uint32_t id, ResourceStoreEntry *entry) {
  if (id < 1) {
    *entry = (ResourceStoreEntry){0};
    return;
  }

  mutex_lock_recursive(s_resource_mutex);

  ListNode *node;
  if (app_num == SYSTEM_APP &&
     (node = list_find((ListNode *)s_resource_list, prv_resource_filter, (void *)(uintptr_t)id))) {
    mutex_unlock_recursive(s_resource_mutex);
    *entry = ((CachedResource *)node)->stored_resource;
    return;
  }

  resource_storage_get_resource(app_num, id, entry);
  mutex_unlock_recursive(s_resource_mutex);
}

//! initialize components needed for one apps resources
bool resource_init_app(ResAppNum app_num, const ResourceVersion *expected_version) {
  // resource_id is ignored in this case, so we set it to 0
  mutex_lock_recursive(s_resource_mutex);
  bool rv = resource_storage_check(app_num, 0, expected_version);
  mutex_unlock_recursive(s_resource_mutex);
  return rv;
}

void resource_init(void) {
  // see if there's a system bank waiting to be loaded
  resource_storage_init();

  s_resource_mutex = mutex_create_recursive();
}

uint32_t resource_get_and_cache(ResAppNum app_num, uint32_t resource_id) {
  PBL_ASSERTN(app_num == SYSTEM_APP);
  // get from resource store
  mutex_lock_recursive(s_resource_mutex);
  ResourceStoreEntry res;
  resource_storage_get_resource(app_num, resource_id, &res);
  if (res.id < 1) {
    mutex_unlock_recursive(s_resource_mutex);
    return 0;
  }

  // check if we already have something in cache for this resource
  CachedResource *cached_resource = (CachedResource *)list_find((ListNode *)s_resource_list,
      prv_resource_filter, (void *)(uintptr_t)resource_id);
  if (cached_resource == NULL) {
    cached_resource = kernel_malloc_check(sizeof(CachedResource));
    *cached_resource = (CachedResource){};
    cached_resource->id = resource_id;
    s_resource_list = (CachedResource *)list_prepend((ListNode *)s_resource_list,
        (ListNode *)cached_resource);
  }
  cached_resource->stored_resource = res;

  mutex_unlock_recursive(s_resource_mutex);
  return resource_id;
}

size_t resource_load_byte_range_system(ResAppNum app_num, uint32_t resource_id,
    uint32_t offset, uint8_t *buffer, size_t num_bytes) {
  PBL_ASSERTN(buffer);

  if (!num_bytes) {
    return 0;
  }

  mutex_lock_recursive(s_resource_mutex);
  ResourceStoreEntry resource;
  prv_get_resource(app_num, resource_id, &resource);
  if (resource.id < 1) {
    mutex_unlock_recursive(s_resource_mutex);
    return 0;
  }

  if (offset + num_bytes > resource.length) {
    if (offset >= resource.length) {
      // Can't recover from trying to read from beyond the resource. Read nothing.
      mutex_unlock_recursive(s_resource_mutex);
      return 0;
    }
    // We want to stop the FW from doing this, so we added an assert
    // but in the name of backwards compatibility, we let the app misbehave
    num_bytes = resource.length - offset;
    PBL_LOG(LOG_LEVEL_DEBUG, "Tried to read past end of resource, reading %d bytes",
            (int)num_bytes);
  }

  size_t bytes_read = resource_storage_read(&resource, offset, buffer, num_bytes);
  mutex_unlock_recursive(s_resource_mutex);
  return bytes_read;
}

size_t resource_size(ResAppNum app_num, uint32_t resource_id) {
  mutex_lock_recursive(s_resource_mutex);
  ResourceStoreEntry resource;
  prv_get_resource(app_num, resource_id, &resource);
  mutex_unlock_recursive(s_resource_mutex);
  return resource.length;
}

bool resource_bytes_are_readonly(void *bytes) {
  return resource_storage_builtin_bytes_are_readonly(bytes) ||
         resource_storage_flash_bytes_are_readonly(bytes);
}

const uint8_t *resource_get_readonly_bytes(ResAppNum app_num, uint32_t resource_id,
                                           size_t *num_bytes_out, ReadonlyBytesFlags flags) {
  // we don't support memory-mapping for resources that don't belong to the system
  if (app_num != SYSTEM_APP) {
    return NULL;
  }

  mutex_lock_recursive(s_resource_mutex);

  // FIXME PBL-28781: This operation touches flash. Even though this is the cleanest approach
  // to detect if the resource is a builtin, it is a slow one. We should instead only search
  // in the builtin table for the resource_ids and if there are no matches, bail early.
  ResourceStoreEntry resource;
  prv_get_resource(app_num, resource_id, &resource);
  mutex_unlock_recursive(s_resource_mutex);

  if (num_bytes_out) {
    *num_bytes_out = resource.length;
  }

  return resource.impl->readonly_bytes(&resource, flags);
}

ResourceVersion resource_get_version(ResAppNum app_num, uint32_t resource_id) {
  mutex_lock_recursive(s_resource_mutex);
  ResourceVersion v = resource_storage_get_version(app_num, resource_id);
  mutex_unlock_recursive(s_resource_mutex);
  return v;
}

ResourceVersion resource_get_system_version(void) {
  return resource_get_version(0, 0);
}

bool resource_is_valid(ResAppNum app_num, uint32_t resource_id) {
  mutex_lock_recursive(s_resource_mutex);
  bool rv = resource_storage_check(app_num, resource_id, NULL /* No expected version */);
  if (rv) {
    ResourceStoreEntry entry;
    prv_get_resource(app_num, resource_id, &entry);
    rv = (entry.id != 0);
  }
  mutex_unlock_recursive(s_resource_mutex);
  return rv;
}

bool resource_version_matches(const ResourceVersion *v1, const ResourceVersion *v2) {
  return (v1->crc == v2->crc);
}

