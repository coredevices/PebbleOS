/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/services/blob_db/timeline_item_storage.h>
#include <pbl/services/time.h>
#include <pbl/util/units.h>

#include <system/status_errno.h>

PBL_LOG_MODULE_DECLARE(service_blob_db, CONFIG_SERVICE_BLOB_DB_LOG_LEVEL);

#define MAX_CHILDREN_PER_PIN 3

// FIRM-1649: temporary instrumentation to catch long mutex hold/wait on the
// timeline item storage mutex. Suspected root cause of multi-second KernelMain
// stalls during QuickView/alarm-save flows is settings_file compaction running
// under this mutex on SysWorkq.
#define FIRM_1649_MUTEX_WARN_MS 100

static uint64_t prv_storage_lock(TimelineItemStorage *storage, const char *op) {
  uint64_t before = pbl_uptime_ticks();
  pbl_mutex_lock(&storage->mutex, PBL_FOREVER);
  uint64_t after = pbl_uptime_ticks();
  uint32_t wait_ms = (uint32_t)(((after - before) * 1000) / PBL_TICK_HZ);
  if (wait_ms >= FIRM_1649_MUTEX_WARN_MS) {
    PBL_LOG_WRN("FIRM-1649: %s waited %" PRIu32 "ms for %s mutex", op, wait_ms, storage->name);
  }
  return after;
}

static void prv_storage_unlock(TimelineItemStorage *storage, uint64_t lock_ticks, const char *op) {
  uint32_t hold_ms = (uint32_t)(((pbl_uptime_ticks() - lock_ticks) * 1000) / PBL_TICK_HZ);
  pbl_mutex_unlock(&storage->mutex);
  if (hold_ms >= FIRM_1649_MUTEX_WARN_MS) {
    PBL_LOG_WRN("FIRM-1649: %s held %s mutex for %" PRIu32 "ms", op, storage->name, hold_ms);
  }
}

typedef struct {
  Uuid parent_id;
  Uuid children_ids[MAX_CHILDREN_PER_PIN];
  int num_children;
  bool find_all;
} FindChildrenInfo;

typedef struct {
  time_t current;
  time_t best;
  Uuid id;
  uint32_t max_age;
  TimelineItemStorageFilterCallback filter_cb;
  bool found;
} NextInfo;

typedef struct {
  time_t earliest;
} GcInfo;

typedef struct {
  bool empty;
} AnyInfo;

// callback for pbl_settings_each that finds the first item by timestamp
static bool prv_each_first_item(struct pbl_settings_file *file, struct pbl_settings_record *info,
                                void *context) {
  if (info->val_len < sizeof(SerializedTimelineItemHeader) ||
      info->key_len != UUID_SIZE) { // deleted or malformed values
    if (info->key_len != UUID_SIZE) {
      PBL_LOG_WRN("Found reminder with invalid key size %u; ignoring.", (unsigned)info->key_len);
    }
    return true;
  }

  SerializedTimelineItemHeader hdr;
  pbl_settings_read_val(file, &hdr, sizeof(SerializedTimelineItemHeader));
  // Restore flags & status
  hdr.common.flags = ~hdr.common.flags;
  hdr.common.status = ~hdr.common.status;

  NextInfo *next_info = (NextInfo *)context;
  time_t timestamp = timeline_item_get_tz_timestamp(&hdr.common);

  // check if filter callback exists, then check if we should keep the item.
  bool filtered_out = false;
  if (next_info->filter_cb && !next_info->filter_cb(&hdr, context)) {
    filtered_out = true;
  }

  if (!filtered_out && (timestamp < next_info->best || !next_info->found) &&
      (timestamp >= next_info->current - (time_t)next_info->max_age)) {
    next_info->found = true;
    next_info->best = timestamp;
    pbl_settings_read_key(file, (uint8_t *)&next_info->id, sizeof(Uuid));
  }

  return true; // continue iterating
}

static bool prv_each_any_item(struct pbl_settings_file *file, struct pbl_settings_record *info,
                              void *context) {
  if (info->val_len < sizeof(SerializedTimelineItemHeader) || info->key_len != UUID_SIZE) {
    return true; // continue looking
  }

  AnyInfo *anyinfo = context;
  anyinfo->empty = false;

  return false; // we found a valid entry
}

static bool prv_each_find_children(struct pbl_settings_file *file, struct pbl_settings_record *info,
                                   void *context) {
  FindChildrenInfo *find_info = (FindChildrenInfo *)context;
  if (info->val_len < sizeof(SerializedTimelineItemHeader) || info->key_len != UUID_SIZE) {
    // malformed values; deleted values have their lengths set to 0
    if (info->key_len != UUID_SIZE) {
      PBL_LOG_WRN("Found malformed item with invalid key/val sizes; ignoring.");
    }
    return true;
  }

  SerializedTimelineItemHeader hdr;
  pbl_settings_read_val(file, &hdr, sizeof(SerializedTimelineItemHeader));
  // Restore flags & status
  hdr.common.flags = ~hdr.common.flags;
  hdr.common.status = ~hdr.common.status;
  if (uuid_equal(&find_info->parent_id, &hdr.common.parent_id)) {
    find_info->children_ids[find_info->num_children++] = hdr.common.id;
  }

  // continue iterating if more children are expected and we want them all
  return ((find_info->num_children < MAX_CHILDREN_PER_PIN) && find_info->find_all);
}

///////////////////////////////////
// Public API
///////////////////////////////////

bool timeline_item_storage_is_empty(TimelineItemStorage *storage) {
  bool rv = true;

  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  AnyInfo any_info = {.empty = true};
  pbl_settings_each(&storage->file, prv_each_any_item, &any_info);
  rv = any_info.empty;

  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

status_t timeline_item_storage_next_item(TimelineItemStorage *storage, Uuid *id_out,
                                         TimelineItemStorageFilterCallback filter_cb) {
  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  NextInfo next_info = {};
  next_info.current = pbl_realtime();
  next_info.max_age = storage->max_item_age;
  next_info.filter_cb = filter_cb;

  status_t rv;
  pbl_settings_each(&storage->file, prv_each_first_item, &next_info);
  if (!next_info.found) {
    rv = S_NO_MORE_ITEMS;
    goto cleanup;
  }

  *id_out = next_info.id;
  rv = S_SUCCESS;

cleanup:
  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

bool timeline_item_storage_exists_with_parent(TimelineItemStorage *storage, const Uuid *parent_id) {
  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  FindChildrenInfo info = {
    .parent_id = *parent_id,
    .num_children = 0,
    .find_all = false,
  };
  pbl_settings_each(&storage->file, prv_each_find_children, &info);

  prv_storage_unlock(storage, lock_ticks, __func__);
  return info.num_children > 0;
}

status_t timeline_item_storage_delete_with_parent(
    TimelineItemStorage *storage, const Uuid *parent_id,
    TimelineItemStorageChildDeleteCallback child_delete_cb) {
  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  FindChildrenInfo info = {
    .parent_id = *parent_id,
    .num_children = 0,
    .find_all = true,
  };
  pbl_settings_each(&storage->file, prv_each_find_children, &info);

  status_t rv = S_SUCCESS;
  for (int i = 0; i < info.num_children; ++i) {
    const void *key = &info.children_ids[i];
    rv = status_from_errno(pbl_settings_delete(&storage->file, key, sizeof(Uuid)));

    if (rv != S_SUCCESS) {
      goto cleanup;
    }

    if (child_delete_cb) {
      child_delete_cb((Uuid *)key);
    }
  }

cleanup:
  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

//! Caution: CommonTimelineItemHeader .flags & .status are stored inverted and not auto-restored
status_t timeline_item_storage_each(TimelineItemStorage *storage,
                                    TimelineItemStorageEachCallback each, void *data) {
  uint64_t lock_ticks = prv_storage_lock(storage, __func__);
  pbl_settings_each(&storage->file, each, data);
  prv_storage_unlock(storage, lock_ticks, __func__);
  return S_SUCCESS;
}

// Earlier firmware stored items with 32-bit timestamps, in a file under another name that it
// keeps reading. They are copied into the current file at boot, then that file is removed, so
// earlier firmware never reads a 64-bit item. Until it is removed the old file is what counts:
// a migration cut short by a reset or an error starts over at the next boot, and so does one
// after a downgrade let earlier firmware write to the old file again.

typedef struct {
  struct pbl_settings_file *new_file;
  status_t status;
} MigrateInfo;

static bool prv_count_item_cb(struct pbl_settings_file *file, struct pbl_settings_record *info,
                              void *context) {
  if (info->key_len == UUID_SIZE && info->val_len >= (int)TIMELINE_ITEM_HEADER_TIME32_SIZE) {
    (*(int *)context)++;
  }
  return true;
}

// Copies an item stored with a 32-bit timestamp, keeping its sync state and modification time.
static bool prv_migrate_item_cb(struct pbl_settings_file *old_file,
                                struct pbl_settings_record *info, void *context) {
  MigrateInfo *migrate = context;
  if (info->key_len != UUID_SIZE || info->val_len < (int)TIMELINE_ITEM_HEADER_TIME32_SIZE) {
    return true; // skip deleted and malformed records
  }
  uint8_t key[UUID_SIZE];
  pbl_settings_read_key(old_file, key, sizeof(key));
  uint8_t *val = pbl_malloc(info->val_len);
  if (!val) {
    migrate->status = E_OUT_OF_MEMORY;
    return false;
  }
  pbl_settings_read_val(old_file, val, info->val_len);
  size_t wide_len;
  uint8_t *wide = timeline_item_serialized_widen(val, info->val_len, &wide_len);
  pbl_free(val);
  if (!wide) {
    migrate->status = E_OUT_OF_MEMORY;
    return false;
  }
  // Flags and status are stored inverted, which widening leaves alone
  status_t rv = status_from_errno(pbl_settings_set_with_timestamp(
      migrate->new_file, key, sizeof(key), wide, wide_len, info->last_modified));
  pbl_free(wide);
  if ((rv == S_SUCCESS) && !info->dirty) {
    rv = status_from_errno(pbl_settings_mark_synced(migrate->new_file, key, sizeof(key)));
  }
  if (FAILED(rv)) {
    migrate->status = rv;
    return false;
  }
  return true;
}

//! Migrates the items of @p legacy_name into the storage file, leaving it open.
//! @return S_SUCCESS if the storage file is open, S_NO_ACTION_REQUIRED if there was nothing to
//! migrate, an error otherwise.
static status_t prv_migrate_time32(TimelineItemStorage *storage, const char *legacy_name) {
  struct pbl_fs_dirent entry;
  if (pbl_fs_stat(legacy_name, &entry) < 0) {
    return S_NO_ACTION_REQUIRED;
  }
  PBL_LOG_INFO("Migrating %s to %s", legacy_name, storage->name);

  struct pbl_settings_file old_file;
  status_t rv = status_from_errno(
      pbl_settings_open_growable(&old_file, legacy_name, storage->max_size, PBL_KIB(8)));
  if (FAILED(rv)) {
    return rv;
  }

  // Size the new file up front: every item grows by its timestamp
  int items = 0;
  pbl_settings_each(&old_file, prv_count_item_cb, &items);
  const int growth = (int)(sizeof(SerializedTimelineItemHeader) - TIMELINE_ITEM_HEADER_TIME32_SIZE);
  const int needed = old_file.used_space + (items + 1) * growth;

  // Start over from the old file, also after an earlier attempt or a downgrade
  pbl_fs_unlink(storage->name);
  rv = status_from_errno(pbl_settings_open_growable(
      &storage->file, storage->name, MAX((int)storage->max_size, needed), MAX(PBL_KIB(8), needed)));
  if (FAILED(rv)) {
    pbl_settings_close(&old_file);
    return rv;
  }

  MigrateInfo info = {.new_file = &storage->file, .status = S_SUCCESS};
  pbl_settings_each(&old_file, prv_migrate_item_cb, &info);
  pbl_settings_close(&old_file);
  if (FAILED(info.status)) {
    pbl_settings_close(&storage->file);
    pbl_fs_unlink(storage->name);
    return info.status;
  }

  // Every item is in the new file. A reset before this only repeats the migration.
  pbl_fs_unlink(legacy_name);
  return S_SUCCESS;
}

void timeline_item_storage_init(TimelineItemStorage *storage, char *filename,
                                const char *legacy_filename, uint32_t max_size, uint32_t max_age) {
  *storage = (TimelineItemStorage){
    .name = filename,
    .max_size = max_size,
    .max_item_age = max_age,
  };
  pbl_mutex_init(&storage->mutex);

  status_t rv = prv_migrate_time32(storage, legacy_filename);
  if (rv == S_SUCCESS) {
    return;
  }
  if (FAILED(rv)) {
    PBL_LOG_ERR("Could not migrate %s, rv = %" PRId32 "; retrying at next boot", legacy_filename,
                rv);
  }

  rv = status_from_errno(
      pbl_settings_open_growable(&storage->file, storage->name, storage->max_size, PBL_KIB(8)));
  if (FAILED(rv)) {
    PBL_LOG_ERR("Unable to create settings file %s, rv = %" PRId32 "!", filename, rv);
  }
}

void timeline_item_storage_deinit(TimelineItemStorage *storage) {
  pbl_settings_close(&storage->file);
}

status_t timeline_item_storage_compact(TimelineItemStorage *storage) {
  uint64_t lock_ticks = prv_storage_lock(storage, __func__);
  status_t rv = status_from_errno(pbl_settings_compact(&storage->file));
  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

status_t timeline_item_storage_insert(TimelineItemStorage *storage, const uint8_t *key, int key_len,
                                      const uint8_t *val, int val_len, bool mark_as_synced) {
  if (key_len != UUID_SIZE || val_len > PBL_SETTINGS_VAL_MAX_LEN ||
      val_len < (int)sizeof(SerializedTimelineItemHeader)) {
    return E_INVALID_ARGUMENT;
  }

  // Check that the layout has the correct items
  if (!timeline_item_verify_layout_serialized(val, val_len)) {
    PBL_LOG_WRN("Timeline item does not have the correct attributes");
    return E_INVALID_ARGUMENT;
  }

  SerializedTimelineItemHeader *hdr = (SerializedTimelineItemHeader *)val;
  // verify that the item isn't too old
  time_t now = pbl_realtime();
  time_t timestamp = timeline_item_get_tz_timestamp(&hdr->common);
  time_t end_timestamp = timestamp + hdr->common.duration * PBL_SEC_PER_MIN;
  if (end_timestamp < now - (time_t)storage->max_item_age) {
    PBL_LOG_WRN("Rejecting stale timeline item %ld seconds old", (long)(now - timestamp));
    return E_INVALID_OPERATION;
  }

  // Invert flags & status to store on flash.
  // TODO: avoid temporarily modifying the const val buffer.
  hdr->common.flags = ~hdr->common.flags;
  hdr->common.status = ~hdr->common.status;

  uint64_t lock_ticks = prv_storage_lock(storage, __func__);
  status_t rv = status_from_errno(pbl_settings_set(&storage->file, key, key_len, val, val_len));

  // Restore flags & status
  hdr->common.flags = ~hdr->common.flags;
  hdr->common.status = ~hdr->common.status;

  if (mark_as_synced) {
    pbl_settings_mark_synced(&storage->file, key, key_len);
  }

  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

int timeline_item_storage_get_len(TimelineItemStorage *storage, const uint8_t *key, int key_len) {
  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  status_t rv = pbl_settings_get_len(&storage->file, key, key_len);

  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

status_t timeline_item_storage_insert_from_phone(TimelineItemStorage *storage, const uint8_t *key,
                                                 int key_len, const uint8_t *val, int val_len,
                                                 bool mark_as_synced) {
  if (timeline_item_phone_has_time64()) {
    return timeline_item_storage_insert(storage, key, key_len, val, val_len, mark_as_synced);
  }
  if (val_len < (int)TIMELINE_ITEM_HEADER_TIME32_SIZE) {
    return E_INVALID_ARGUMENT;
  }
  size_t wide_len;
  uint8_t *wide = timeline_item_serialized_widen(val, val_len, &wide_len);
  if (!wide) {
    return E_OUT_OF_MEMORY;
  }
  const status_t rv =
      timeline_item_storage_insert(storage, key, key_len, wide, wide_len, mark_as_synced);
  pbl_free(wide);
  return rv;
}

int timeline_item_storage_get_len_for_phone(TimelineItemStorage *storage, const uint8_t *key,
                                            int key_len) {
  const int len = timeline_item_storage_get_len(storage, key, key_len);
  if (timeline_item_phone_has_time64() || len < (int)sizeof(SerializedTimelineItemHeader)) {
    return len;
  }
  return len - (int)(sizeof(SerializedTimelineItemHeader) - TIMELINE_ITEM_HEADER_TIME32_SIZE);
}

status_t timeline_item_storage_read_for_phone(TimelineItemStorage *storage, const uint8_t *key,
                                              int key_len, uint8_t *val_out, int val_len) {
  if (timeline_item_phone_has_time64()) {
    return timeline_item_storage_read(storage, key, key_len, val_out, val_len);
  }
  if (val_len < (int)TIMELINE_ITEM_HEADER_TIME32_SIZE) {
    return E_INVALID_ARGUMENT;
  }
  const int wide_len =
      val_len + (int)(sizeof(SerializedTimelineItemHeader) - TIMELINE_ITEM_HEADER_TIME32_SIZE);
  uint8_t *wide = pbl_malloc(wide_len);
  if (!wide) {
    return E_OUT_OF_MEMORY;
  }
  status_t rv = timeline_item_storage_read(storage, key, key_len, wide, wide_len);
  if (rv == S_SUCCESS) {
    timeline_item_serialized_narrow(wide, wide_len);
    memcpy(val_out, wide, val_len);
  }
  pbl_free(wide);
  return rv;
}

status_t timeline_item_storage_read(TimelineItemStorage *storage, const uint8_t *key, int key_len,
                                    uint8_t *val_out, int val_len) {
  if (key_len != UUID_SIZE) {
    return E_INVALID_ARGUMENT;
  }

  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  status_t rv = status_from_errno(pbl_settings_get(&storage->file, key, key_len, val_out, val_len));

  // Restore flags & status
  SerializedTimelineItemHeader *hdr = (SerializedTimelineItemHeader *)val_out;
  hdr->common.flags = ~hdr->common.flags;
  hdr->common.status = ~hdr->common.status;

  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

status_t timeline_item_storage_get_from_settings_record(struct pbl_settings_file *file,
                                                        struct pbl_settings_record *info,
                                                        TimelineItem *item) {
  uint8_t *read_buf = pbl_zalloc_check(info->val_len);
  pbl_settings_read_val(file, read_buf, info->val_len);

  SerializedTimelineItemHeader *header = (SerializedTimelineItemHeader *)read_buf;
  // Restore flags & status
  header->common.flags = ~header->common.flags;
  header->common.status = ~header->common.status;
  uint8_t *payload = read_buf + sizeof(SerializedTimelineItemHeader);
  status_t rv = timeline_item_deserialize_item(item, header, payload) ? S_SUCCESS : E_INTERNAL;

  pbl_free(read_buf);
  return rv;
}

status_t timeline_item_storage_set_status_bits(TimelineItemStorage *storage, const uint8_t *key,
                                               int key_len, uint8_t status) {
  if (key_len != UUID_SIZE) {
    return E_INVALID_ARGUMENT;
  }

  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  int offset = offsetof(SerializedTimelineItemHeader, common.status);
  // Invert status to store on flash
  status = ~status;
  status_t rv =
      status_from_errno(pbl_settings_set_byte(&storage->file, key, key_len, offset, status));

  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

status_t timeline_item_storage_delete(TimelineItemStorage *storage, const uint8_t *key,
                                      int key_len) {
  if (key_len != UUID_SIZE) {
    return E_INVALID_ARGUMENT;
  }

  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  status_t rv = status_from_errno(pbl_settings_delete(&storage->file, key, key_len));

  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

status_t timeline_item_storage_mark_synced(TimelineItemStorage *storage, const uint8_t *key,
                                           int key_len) {
  if (key_len == 0) {
    return E_INVALID_ARGUMENT;
  }

  uint64_t lock_ticks = prv_storage_lock(storage, __func__);

  status_t rv = status_from_errno(pbl_settings_mark_synced(&storage->file, key, key_len));

  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}

static bool prv_flush_filter_cb(void *key, size_t key_len, void *val, size_t val_len,
                                void *context) {
  if (key_len != sizeof(Uuid) || val_len < sizeof(SerializedTimelineItemHeader)) {
    return false;
  }

  // Flags are stored inverted
  const SerializedTimelineItemHeader *hdr = val;
  return (~hdr->common.flags & TimelineItemFlagFromWatch) != 0;
}

status_t timeline_item_storage_flush(TimelineItemStorage *storage) {
  uint64_t lock_ticks = prv_storage_lock(storage, __func__);
  status_t rv = status_from_errno(
      pbl_settings_rewrite_filtered(&storage->file, prv_flush_filter_cb, nullptr));
  prv_storage_unlock(storage, lock_ticks, __func__);
  return rv;
}
