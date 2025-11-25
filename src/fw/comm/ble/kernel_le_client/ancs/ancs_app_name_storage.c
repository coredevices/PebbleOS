/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ancs_app_name_storage.h"

#include <string.h>

#include "util/circular_cache.h"
#include "kernel/pbl_malloc.h"
#include "system/passert.h"

static const unsigned ANCS_APP_NAME_STORAGE_SIZE = 30;

static CircularCache *s_cache = NULL;

typedef struct {
  ANCSAttribute *app_id;
  ANCSAttribute *app_name;
} AncsAppNameStorageEntry;

static int prv_comparator(void *a, void *b) {
  AncsAppNameStorageEntry *entry_a = (AncsAppNameStorageEntry *)a;
  AncsAppNameStorageEntry *entry_b = (AncsAppNameStorageEntry *)b;

  if (entry_a->app_id == NULL || entry_b->app_id == NULL) {
    return -1;
  }

  if (entry_a->app_id->length != entry_b->app_id->length) {
    return -1;
  }

  return (memcmp(entry_a->app_id->value, entry_b->app_id->value, entry_a->app_id->length));
}

static void prv_destructor(void *item) {
  if (item) {
    AncsAppNameStorageEntry *entry = item;
    kernel_free(entry->app_id);
    kernel_free(entry->app_name);
  }
}

void ancs_app_name_storage_init(void) {
  PBL_ASSERTN(!s_cache);
  s_cache = kernel_zalloc_check(sizeof(*s_cache));
  uint8_t *buffer =
      kernel_zalloc_check(sizeof(AncsAppNameStorageEntry) * ANCS_APP_NAME_STORAGE_SIZE);
  circular_cache_init(s_cache,
                      buffer,
                      sizeof(AncsAppNameStorageEntry),
                      ANCS_APP_NAME_STORAGE_SIZE,
                      prv_comparator);
  circular_cache_set_item_destructor(s_cache, prv_destructor);
}


void ancs_app_name_storage_deinit(void) {
  if (s_cache) {
    circular_cache_flush(s_cache);
    kernel_free(s_cache->cache);
    kernel_free(s_cache);
    s_cache = NULL;
  }
}

void ancs_app_name_storage_store(const ANCSAttribute *app_id, const ANCSAttribute *app_name) {
  if (!app_id || !app_name) {
    return;
  }

  // copy app id
  ANCSAttribute *app_id_copy = kernel_zalloc_check(sizeof(ANCSAttribute) + app_id->length);;
  memcpy(app_id_copy, app_id, sizeof(ANCSAttribute) + app_id->length);
  // copy app name
  ANCSAttribute *app_name_copy = kernel_zalloc_check(sizeof(ANCSAttribute) + app_name->length);;
  memcpy(app_name_copy, app_name, sizeof(ANCSAttribute) + app_name->length);
  AncsAppNameStorageEntry entry = {
    .app_id = app_id_copy,
    .app_name = app_name_copy,
  };
  circular_cache_push(s_cache, &entry);
}

ANCSAttribute *ancs_app_name_storage_get(const ANCSAttribute *app_id) {
  AncsAppNameStorageEntry entry = {
    .app_id = (ANCSAttribute *)app_id,
  };
  AncsAppNameStorageEntry *found = circular_cache_get(s_cache, &entry);
  if (found) {
    return found->app_name;
  } else {
    return NULL;
  }
}
