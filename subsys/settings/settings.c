/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "raw_iter.h"

#include <errno.h>
#include <string.h>
#include <time.h>

#include <pbl/crc/crc.h>
#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/settings/settings.h>
#include <pbl/task_wdt/task_wdt.h>

PBL_LOG_MODULE_DEFINE(settings, CONFIG_SETTINGS_LOG_LEVEL);

static struct pbl_settings_watch *s_watches;

static int bootup_check(struct pbl_settings_file *file);
static void compute_stats(struct pbl_settings_file *file);

static bool file_hdr_is_uninitialized(struct pbl_settings_file_hdr *file_hdr) {
  return (file_hdr->magic == 0xffffffff) && (file_hdr->version == 0xffff) &&
         (file_hdr->flags == 0xffff);
}

static int prv_close_and_remove(struct pbl_settings_file *file, const char *name) {
  pbl_fs_close(&file->iter.file);
  return pbl_fs_unlink(name);
}

static int prv_open(struct pbl_settings_file *file, const char *name, uint32_t flags,
                    int max_used_space, int alloc_used_space, int min_alloc_used_space) {
  // Making the max_space_total at least a little bit larger than the
  // alloc_used_space allows us to avoid thrashing. Without it, if
  // max_space_total == alloc_used_space, then if the file is full, changing a
  // single value would force the whole file to be rewritten- every single
  // time! It's probably worth it to "waste" a bit of flash space to avoid
  // this pathological case.
  int max_space_total = pbl_fs_pfs_optimal_size(name, alloc_used_space * 12 / 10);
  if (max_space_total < 0) {
    return max_space_total;
  }

  struct pbl_fs_file fs_file;
  int ret = pbl_fs_open(&fs_file, name, flags);
  if ((ret == 0) && (pbl_fs_size(&fs_file) == 0)) {
    ret = pbl_fs_truncate(&fs_file, max_space_total);
    if (ret < 0) {
      pbl_fs_close(&fs_file);
    }
  }

  if (ret < 0) {
    PBL_LOG_ERR("Could not open settings file '%s', %d", name, ret);
    if (ret == -EBUSY) {
      // This is very bad. Someone didn't use a mutex. There could already be
      // silent corruption, so it's better to crash now rather than let things
      // get even more scrambled.
      pbl_panic("Settings file is already open!");
    }
    return ret;
  }

  *file = (struct pbl_settings_file){
    .name = pbl_strdup_check(name),
    .max_used_space = max_used_space,
    .alloc_used_space = alloc_used_space,
    .min_alloc_used_space = min_alloc_used_space,
    .max_space_total = max_space_total,
  };

  pbl_settings__raw_iter_init(&file->iter, &fs_file, file->name);

  struct pbl_settings_file_hdr file_hdr = file->iter.file_hdr;
  if (file_hdr_is_uninitialized(&file_hdr)) {
    // Newly created file, create & write out header.
    memcpy(&file_hdr.magic, SETTINGS_FILE_MAGIC, sizeof(file_hdr.magic));
    file_hdr.version = SETTINGS_FILE_VERSION;
    pbl_settings__raw_iter_write_file_header(&file->iter, &file_hdr);
  }

  if (memcmp(&file_hdr.magic, SETTINGS_FILE_MAGIC, sizeof(file_hdr.magic)) != 0) {
    PBL_LOG_ERR("Attempted to open %s, not a settings file. Removing and recreating.", name);
    int rv = prv_close_and_remove(file, name);
    if (rv < 0) {
      PBL_LOG_ERR("Could not remove corrupt settings file %s: %d", name, rv);
      return rv;
    }
    return prv_open(file, name, flags, max_used_space, alloc_used_space, min_alloc_used_space);
  }

  if (file_hdr.version > SETTINGS_FILE_VERSION) {
    PBL_LOG_WRN("Unrecognized version %d for file %s, removing...", file_hdr.version, name);
    int rv = prv_close_and_remove(file, name);
    if (rv < 0) {
      PBL_LOG_ERR("Could not remove old-version settings file %s: %d", name, rv);
      return rv;
    }
    return prv_open(file, name, flags, max_used_space, alloc_used_space, min_alloc_used_space);
  }

  // For growable files, adopt the actual file size before bootup_check so that
  // any compaction during recovery uses the correct (grown) allocation size.
  int actual_size = pbl_fs_size(&file->iter.file);
  if (alloc_used_space < max_used_space && actual_size > max_space_total) {
    file->alloc_used_space = actual_size * 10 / 12;
    file->max_space_total = actual_size;
  }

  ret = bootup_check(file);
  if (ret < 0) {
    PBL_LOG_ERR(
        "Bootup check failed (%d"
        "), not good. "
        "Attempting to recover by deleting %s...",
        ret, name);
    prv_close_and_remove(file, name);
    return prv_open(file, name, flags, max_used_space, alloc_used_space, min_alloc_used_space);
  }

  // There's a chance that the caller increased the desired size of the settings file since
  // the file was originally created (i.e. the file was created in an earlier version of the
  // firmware). If we detect that situation, let's re-write the file to the new larger requested
  // size.
  if (alloc_used_space >= max_used_space && actual_size < max_space_total) {
    PBL_LOG_DBG("Re-writing settings file %s to increase its size from %d to %d.", name,
                actual_size, max_space_total);
    ret = pbl_settings_rewrite_filtered(file, nullptr, nullptr);
    if (ret < 0) {
      PBL_LOG_ERR("Could not resize file %s (error %d). Creating new one", name, ret);
      return prv_open(file, name, flags, max_used_space, alloc_used_space, min_alloc_used_space);
    }
  }

  compute_stats(file);

  return 0;
}

int pbl_settings_open(struct pbl_settings_file *file, const char *name, int max_used_space) {
  return prv_open(file, name, PBL_FS_O_RDWR | PBL_FS_O_CREATE, max_used_space, max_used_space,
                  max_used_space);
}

int pbl_settings_open_growable(struct pbl_settings_file *file, const char *name, int max_used_space,
                               int initial_alloc_size) {
  // prv_grow doubles alloc_used_space; a zero seed would loop forever.
  PBL_ASSERTN(initial_alloc_size > 0);
  return prv_open(file, name, PBL_FS_O_RDWR | PBL_FS_O_CREATE, max_used_space, initial_alloc_size,
                  initial_alloc_size);
}

void pbl_settings_close(struct pbl_settings_file *file) {
  pbl_settings__raw_iter_deinit(&file->iter);
  pbl_free(file->name);
  file->name = nullptr;
}

static int record_size(struct pbl_settings_record_hdr *hdr) {
  return sizeof(*hdr) + hdr->key_len + hdr->val_len;
}

// Flags are stored in flash the inverse of how you might normally expect- a
// zero denotes that the flag is set, a 1 means it is not. This is because our
// flash chip is NOR flash, and thus is all 1's by default.
// Once setting a flag, we cannot unset it.
static void set_flag(struct pbl_settings_record_hdr *hdr, uint8_t flags) {
  hdr->flags &= ~flags;
}
static void clear_flag(struct pbl_settings_record_hdr *hdr, uint8_t flags) {
  hdr->flags |= flags;
}
static bool flag_is_set(struct pbl_settings_record_hdr *hdr, uint8_t flags) {
  return (hdr->flags & flags) == 0;
}

// Records have 4 possible states:
// - EOF marker: Header is all 1s
// - partially_written: Some bits in the header have been changed to 0s, but
//     the entire record has not been completely written yet. Records in this
//     state are removed on bootup, since they are in an indeterminate state.
// - written: The typical state for a record. == !partially_written
// - partially_overwritten: This record has been superseded by another, which
//     we are currently in the process of writing out to flash. Records in
//     this state are restored on bootup.
// - overwritten: This record has been superseded by another, which has been
//     completely written out to flash. We skip over and ignore overwritten
//     records.
static bool partially_written(struct pbl_settings_record_hdr *hdr) {
  return !flag_is_set(hdr, SETTINGS_FLAG_WRITE_COMPLETE);
}
static bool partially_overwritten(struct pbl_settings_record_hdr *hdr) {
  return flag_is_set(hdr, SETTINGS_FLAG_OVERWRITE_STARTED) &&
         !flag_is_set(hdr, SETTINGS_FLAG_OVERWRITE_COMPLETE);
}
static bool overwritten(struct pbl_settings_record_hdr *hdr) {
  return flag_is_set(hdr, SETTINGS_FLAG_OVERWRITE_STARTED) &&
         flag_is_set(hdr, SETTINGS_FLAG_OVERWRITE_COMPLETE);
}

static uint32_t utc_time() {
  return pbl_realtime();
}

static bool deleted(struct pbl_settings_record_hdr *hdr) {
  return hdr->val_len == 0;
}

static void compute_stats(struct pbl_settings_file *file) {
  file->dead_space = 0;
  file->used_space = 0;
  file->last_modified = 0;
  file->used_space += sizeof(struct pbl_settings_file_hdr);
  file->used_space += sizeof(struct pbl_settings_record_hdr); // EOF Marker
  for (pbl_settings__raw_iter_begin(&file->iter); !pbl_settings__raw_iter_end(&file->iter);
       pbl_settings__raw_iter_next(&file->iter)) {
    if (overwritten(&file->iter.hdr) || deleted(&file->iter.hdr)) {
      file->dead_space += record_size(&file->iter.hdr);
    } else {
      file->used_space += record_size(&file->iter.hdr);
    }
    if (file->iter.hdr.last_modified > file->last_modified) {
      file->last_modified = file->iter.hdr.last_modified;
    }
  }
}

int pbl_settings_rewrite_filtered(struct pbl_settings_file *file,
                                  pbl_settings_filter_cb_t filter_cb, void *context) {
  // One reusable buffer for key+val per record; sized for the worst case.
  // Avoids two malloc/free pairs per record over what can be thousands of
  // records on a large persist file.
  uint8_t *kv_buf = pbl_malloc(PBL_SETTINGS_KEY_MAX_LEN + PBL_SETTINGS_VAL_MAX_LEN);
  char *name = pbl_strdup(file->name);
  if (!kv_buf || !name) {
    PBL_LOG_ERR("Could not allocate buffers to compact settings file %s", file->name);
    pbl_free(kv_buf);
    pbl_free(name);
    return -ENOMEM;
  }

  // A 1 MiB grow can take many seconds of pure flash erase + write time.
  pbl_task_wdt_suspend(60 * 1000);

  struct pbl_settings_file new_file;
  int ret = prv_open(&new_file, file->name, PBL_FS_O_REPLACE | PBL_FS_O_READ, file->max_used_space,
                     file->alloc_used_space, file->min_alloc_used_space);
  if (ret < 0) {
    PBL_LOG_ERR("Could not open temporary file to compact settings file. Error %d.", ret);
    pbl_free(kv_buf);
    pbl_free(name);
    pbl_task_wdt_resume();
    return ret;
  }

  pbl_settings__raw_iter_begin(&new_file.iter);

  for (pbl_settings__raw_iter_begin(&file->iter); !pbl_settings__raw_iter_end(&file->iter);
       pbl_settings__raw_iter_next(&file->iter)) {
    struct pbl_settings_record_hdr *hdr = &file->iter.hdr;
    if (partially_written(hdr)) {
      // This should only happen if we reboot in the middle of writing a new
      // record, and it should always be the most recently written record, so
      // we shouldn't lose any data here (except for the partially written
      // record, but we should ignore it's garbage data anyway).
      break;
    }
    if (overwritten(hdr) || deleted(hdr)) {
      continue;
    }
    if (partially_overwritten(hdr)) {
      // The only case where we should hit this is if we are compacting a file
      // which has a record which was in the middle of being overwritten, but
      // the write of the new record didn't finish by the time we rebooted.
      // There should only ever be one such record, and we shouldn't ever hit
      // this if writing the new record actually completed, since we check
      // for this case in bootup_check().
      clear_flag(hdr, SETTINGS_FLAG_OVERWRITE_STARTED);
    }

    // Read key+val in a single PFS call; key occupies the first key_len bytes.
    pbl_settings__raw_iter_read_key_val(&file->iter, kv_buf);
    uint8_t *key = kv_buf;
    uint8_t *val = kv_buf + hdr->key_len;

    // Include in re-written file if it passes the filter
    if (!filter_cb || filter_cb(key, hdr->key_len, val, hdr->val_len, context)) {
      pbl_settings__raw_iter_write_header(&new_file.iter, hdr);
      pbl_settings__raw_iter_write_key_val(&new_file.iter, kv_buf);
      pbl_settings__raw_iter_next(&new_file.iter);
    }
  }
  pbl_free(kv_buf);
  pbl_settings_close(file);
  // We have to close and reopen the new_file so that it's temp flag is cleared.
  // Before the close succeeds, if we reboot, we will just end up reading the
  // old file. After the close succeeds, we will end up reading the new
  // (compacted) file.
  int alloc_used_space = new_file.alloc_used_space;
  int min_alloc_used_space = new_file.min_alloc_used_space;
  pbl_settings_close(&new_file);
  ret = prv_open(file, name, PBL_FS_O_RDWR | PBL_FS_O_CREATE, file->max_used_space,
                 alloc_used_space, min_alloc_used_space);
  pbl_free(name);

  pbl_task_wdt_resume();

  return ret;
}

void pbl_settings_watch_add(struct pbl_settings_watch *watch) {
  watch->next = s_watches;
  s_watches = watch;
}

int pbl_settings_compact(struct pbl_settings_file *file) {
  // For growable files, drop alloc_used_space toward the floor when live data
  // is much smaller than the current allocation. Without this, a file that
  // burst to e.g. 256 KiB and then idled would hold that flash forever even
  // after the records were deleted, slowly bleeding free PFS space across
  // device lifetime. Aim for the smallest doubling step that leaves ~2x
  // headroom over used_space; the next grow cycle will re-expand if needed.
  const int old_alloc = file->alloc_used_space;
  if (file->alloc_used_space > file->min_alloc_used_space) {
    int target = file->min_alloc_used_space;
    while (target < file->used_space * 2 && target < file->alloc_used_space) {
      target *= 2;
    }
    if (target < file->alloc_used_space) {
      file->alloc_used_space = target;
    }
  }
  int ret = pbl_settings_rewrite_filtered(file, nullptr, nullptr);
  if (ret < 0) {
    // rewrite_filtered fails before the swap if it fails at all; the on-disk
    // file is still at old_alloc, so put the in-memory book-keeping back.
    file->alloc_used_space = old_alloc;
  }
  return ret;
}

static bool key_matches(struct pbl_settings_raw_iter *iter, const uint8_t *key, int key_len) {
  struct pbl_settings_record_hdr *hdr = &iter->hdr;
  if (key_len != hdr->key_len) {
    return false;
  }
  if (pbl_crc8_reversed(0, key, key_len) != hdr->key_hash) {
    return false;
  }
  uint8_t hdr_key[hdr->key_len];
  pbl_settings__raw_iter_read_key(iter, hdr_key);
  if (memcmp(key, hdr_key, hdr->key_len) == 0) {
    return true;
  }
  return false;
}

static bool prv_is_desired_hdr(struct pbl_settings_raw_iter *iter, const uint8_t *key,
                               int key_len) {
  if (overwritten(&iter->hdr) || partially_written(&iter->hdr)) {
    return false;
  }

  return key_matches(iter, key, key_len);
}

static bool search_forward(struct pbl_settings_raw_iter *iter, const uint8_t *key, int key_len) {
  const int resumed_pos = pbl_settings__raw_iter_get_resumed_record_pos(iter);

  // Resume searching at the current record
  for (; !pbl_settings__raw_iter_end(iter); pbl_settings__raw_iter_next(iter)) {
    if (prv_is_desired_hdr(iter, key, key_len)) {
      return true;
    }
  }

  // If we got here, we didn't find it between `resumed_pos` and the last record in the file.
  // Wrap around to the beginning and search until we get to the `resumed_pos`
  pbl_settings__raw_iter_begin(iter);
  for (; pbl_settings__raw_iter_get_current_record_pos(iter) < resumed_pos;
       pbl_settings__raw_iter_next(iter)) {
    if (prv_is_desired_hdr(iter, key, key_len)) {
      return true;
    }
  }

  // No record found
  return false;
}

static int cleanup_partial_transactions(struct pbl_settings_file *file) {
  for (pbl_settings__raw_iter_begin(&file->iter); !pbl_settings__raw_iter_end(&file->iter);
       pbl_settings__raw_iter_next(&file->iter)) {
    if (partially_written(&file->iter.hdr)) {
      // Compact will remove partially written records. We could be smarter,
      // but this is something of an edge case.
      return pbl_settings_compact(file);
    }

    if (!partially_overwritten(&file->iter.hdr)) {
      continue;
    }

    int partially_overwritten_record_pos =
        pbl_settings__raw_iter_get_current_record_pos(&file->iter);
    uint8_t key[file->iter.hdr.key_len];
    pbl_settings__raw_iter_read_key(&file->iter, key);
    pbl_settings__raw_iter_next(&file->iter); // Skip the current record
    bool found_another = search_forward(&file->iter, key, file->iter.hdr.key_len);

    if (!found_another) {
      // No other file->iter.hdr found, we must have rebooted in the middle of
      // writing the new record. Compacting the file will copy over the
      // previous record while clearing the overwrite bits for us, so that we
      // can still find the previous record. We could be smarter here, but
      // this should happen pretty rarely.
      return pbl_settings_compact(file);
    }

    // The overwrite completed, we just rebooted before getting a chance
    // to flip the completion bit on the previous record. Flip it now so
    // that we don't have to keep checking on every boot.
    pbl_settings__raw_iter_set_current_record_pos(&file->iter, partially_overwritten_record_pos);
    set_flag(&file->iter.hdr, SETTINGS_FLAG_OVERWRITE_COMPLETE);
    pbl_settings__raw_iter_write_header(&file->iter, &file->iter.hdr);
  }
  return 0;
}

static int bootup_check(struct pbl_settings_file *file) {
  return cleanup_partial_transactions(file);
}

size_t pbl_settings_get_len(struct pbl_settings_file *file, const void *key, size_t key_len) {
  pbl_settings__raw_iter_resume(&file->iter);
  if (search_forward(&file->iter, key, key_len)) {
    return file->iter.hdr.val_len;
  } else {
    return 0;
  }
}

bool pbl_settings_exists(struct pbl_settings_file *file, const void *key, size_t key_len) {
  return (pbl_settings_get_len(file, key, key_len) > 0);
}

int pbl_settings_get(struct pbl_settings_file *file, const void *key, size_t key_len, void *val_out,
                     size_t val_out_len) {
  pbl_settings__raw_iter_resume(&file->iter);
  if (!search_forward(&file->iter, key, key_len)) {
    memset(val_out, 0, val_out_len);
    return -ENOENT;
  }
  if (deleted(&file->iter.hdr)) {
    memset(val_out, 0, val_out_len);
    return -ENOENT;
  }
  size_t val_len = file->iter.hdr.val_len;
  if (val_out_len > val_len) {
    memset(val_out, 0, val_out_len);
    return -ERANGE;
  }
  pbl_settings__raw_iter_read_val(&file->iter, val_out, val_out_len);
  return 0;
}

int pbl_settings_set_byte(struct pbl_settings_file *file, const void *key, size_t key_len,
                          size_t offset, uint8_t byte) {
  if (key_len > PBL_SETTINGS_KEY_MAX_LEN) {
    return -ERANGE;
  }

  // Find the record
  pbl_settings__raw_iter_resume(&file->iter);
  if (!search_forward(&file->iter, key, key_len) || file->iter.hdr.val_len == 0) {
    return -ENOENT;
  }

  PBL_ASSERTN(offset < file->iter.hdr.val_len);
  pbl_settings__raw_iter_write_byte(&file->iter, offset, byte);

  return 0;
}

static int prv_grow(struct pbl_settings_file *file, int needed_used_space) {
  int new_alloc = file->alloc_used_space;
  while (new_alloc < needed_used_space && new_alloc < file->max_used_space) {
    new_alloc *= 2;
  }
  if (new_alloc > file->max_used_space) {
    new_alloc = file->max_used_space;
  }
  if (new_alloc < needed_used_space) {
    return -ENOSPC;
  }

  int old_alloc = file->alloc_used_space;
  file->alloc_used_space = new_alloc;
  int ret = pbl_settings_rewrite_filtered(file, nullptr, nullptr);
  if (ret < 0) {
    file->alloc_used_space = old_alloc;
  }
  return ret;
}

// Internal implementation that takes a timestamp parameter
// Note that this operation is designed to be atomic from the perspective of
// an outside observer. That is, either the new value will be completely
// written and returned for all future queries, or, if we reboot/loose power/
// run into an error, then we will continue to return the previous value.
// We should never run into a case where neither value exists.
static int prv_settings_file_set_internal(struct pbl_settings_file *file, const void *key,
                                          size_t key_len, const void *val, size_t val_len,
                                          uint32_t timestamp) {
  // Cannot set keys while iterating (Try pbl_settings_rewrite)
  PBL_ASSERTN(file->cur_record_pos == 0);
  if (key_len > PBL_SETTINGS_KEY_MAX_LEN) {
    return -ERANGE;
  }
  if (val_len > PBL_SETTINGS_VAL_MAX_LEN) {
    return -ERANGE;
  }
  const bool is_delete = (val_len == 0);
  const int rec_size = sizeof(struct pbl_settings_record_hdr) + key_len + val_len;
  if (!is_delete && file->used_space + rec_size > file->max_used_space) {
    return -ENOSPC;
  }
  if (file->used_space + file->dead_space + rec_size > file->max_space_total) {
    bool needs_growth = (file->used_space + rec_size > file->max_space_total) &&
                        (file->alloc_used_space < file->max_used_space);
    int ret;
    if (needs_growth) {
      ret = prv_grow(file, file->used_space + rec_size);
    } else {
      ret = pbl_settings_compact(file);
    }
    if (ret < 0) {
      return ret;
    }
  }

  int overwritten_record = -1;
  // Find an existing record, if any, and mark it as overwrite-in-progress.
  pbl_settings__raw_iter_resume(&file->iter);
  if (search_forward(&file->iter, key, key_len)) {
    set_flag(&file->iter.hdr, SETTINGS_FLAG_OVERWRITE_STARTED);
    pbl_settings__raw_iter_write_header(&file->iter, &file->iter.hdr);
    overwritten_record = pbl_settings__raw_iter_get_current_record_pos(&file->iter);
  }

  while (!pbl_settings__raw_iter_end(&file->iter)) {
    pbl_settings__raw_iter_next(&file->iter);
  }

  // Create and write out a new record. Writing the header transitions us into
  // the write-in-progress state, since at least once of the bits must be
  // flipped from a 1 to a 0 in order for the header to be valid.
  struct pbl_settings_record_hdr new_hdr;
  memset(&new_hdr, 0xff, sizeof(new_hdr));
  new_hdr.last_modified = timestamp;
  new_hdr.key_hash = pbl_crc8_reversed(0, key, key_len);
  new_hdr.key_len = key_len;
  new_hdr.val_len = val_len;

  pbl_settings__raw_iter_write_header(&file->iter, &new_hdr);
  pbl_settings__raw_iter_write_key(&file->iter, key);
  pbl_settings__raw_iter_write_val(&file->iter, val);

  // Mark the new record as write complete, now that we have completely written
  // out the header, key, and value.
  set_flag(&new_hdr, SETTINGS_FLAG_WRITE_COMPLETE);
  pbl_settings__raw_iter_write_header(&file->iter, &new_hdr);
  file->used_space += rec_size;

  // Finally, mark the existing record, if any, as overwritten.
  if (overwritten_record >= 0) {
    pbl_settings__raw_iter_set_current_record_pos(&file->iter, overwritten_record);
    set_flag(&file->iter.hdr, SETTINGS_FLAG_OVERWRITE_COMPLETE);
    pbl_settings__raw_iter_write_header(&file->iter, &file->iter.hdr);
    file->dead_space += record_size(&file->iter.hdr);
    file->used_space -= record_size(&file->iter.hdr);
  }

  for (struct pbl_settings_watch *w = s_watches; w != nullptr; w = w->next) {
    if (strcmp(w->name, file->name) == 0) {
      w->cb(key, key_len, new_hdr.last_modified, w->user_data);
    }
  }

  return 0;
}

int pbl_settings_set(struct pbl_settings_file *file, const void *key, size_t key_len,
                     const void *val, size_t val_len) {
  return prv_settings_file_set_internal(file, key, key_len, val, val_len, utc_time());
}

int pbl_settings_mark_synced(struct pbl_settings_file *file, const void *key, size_t key_len) {
  // Cannot set keys while iterating (Try pbl_settings_rewrite)
  PBL_ASSERTN(file->cur_record_pos == 0);
  if (key_len > PBL_SETTINGS_KEY_MAX_LEN) {
    return -ERANGE;
  }

  // Find an existing record, if any, and mark it as synced
  pbl_settings__raw_iter_resume(&file->iter);
  if (search_forward(&file->iter, key, key_len)) {
    set_flag(&file->iter.hdr, SETTINGS_FLAG_SYNCED);
    pbl_settings__raw_iter_write_header(&file->iter, &file->iter.hdr);
    return 0;
  }

  return -ENOENT;
}

static void prv_mark_all_dirty_rewrite_cb(struct pbl_settings_file *old_file,
                                          struct pbl_settings_file *new_file,
                                          struct pbl_settings_record *info, void *context) {
  // Read key and value from old file
  uint8_t key[PBL_SETTINGS_KEY_MAX_LEN];
  pbl_settings_read_key(old_file, key, info->key_len);

  uint8_t *val = pbl_malloc(info->val_len);
  if (!val) {
    return; // Skip this record on OOM
  }
  pbl_settings_read_val(old_file, val, info->val_len);

  // Write to new file with original timestamp - records are dirty by default (no SYNCED flag)
  // Preserving the timestamp ensures sync conflict resolution works correctly
  prv_settings_file_set_internal(new_file, key, info->key_len, val, info->val_len,
                                 info->last_modified);
  pbl_free(val);
}

int pbl_settings_mark_all_dirty(struct pbl_settings_file *file) {
  return pbl_settings_rewrite(file, prv_mark_all_dirty_rewrite_cb, nullptr);
}

int pbl_settings_delete(struct pbl_settings_file *file, const void *key, size_t key_len) {
  return pbl_settings_set(file, key, key_len, nullptr, 0);
}

void pbl_settings_read_key(struct pbl_settings_file *file, void *key, size_t key_len) {
  PBL_ASSERTN(key_len <= file->iter.hdr.key_len);
  pbl_settings__raw_iter_set_current_record_pos(&file->iter, file->cur_record_pos);
  pbl_settings__raw_iter_read_key(&file->iter, key);
}
void pbl_settings_read_val(struct pbl_settings_file *file, void *val, size_t val_len) {
  PBL_ASSERTN(val_len <= file->iter.hdr.val_len);
  pbl_settings__raw_iter_set_current_record_pos(&file->iter, file->cur_record_pos);
  pbl_settings__raw_iter_read_val(&file->iter, val, val_len);
}
void pbl_settings_each(struct pbl_settings_file *file, pbl_settings_each_cb_t cb, void *context) {
  // Cannot set keys while iterating
  PBL_ASSERTN(file->cur_record_pos == 0);
  struct pbl_settings_record info;
  for (pbl_settings__raw_iter_begin(&file->iter); !pbl_settings__raw_iter_end(&file->iter);
       pbl_settings__raw_iter_next(&file->iter)) {
    if (overwritten(&file->iter.hdr) || deleted(&file->iter.hdr)) {
      continue;
    }
    info = (struct pbl_settings_record){
      .last_modified = file->iter.hdr.last_modified,
      .key_len = file->iter.hdr.key_len,
      .val_len = file->iter.hdr.val_len,
      .dirty = !flag_is_set(&file->iter.hdr, SETTINGS_FLAG_SYNCED),
    };
    file->cur_record_pos = pbl_settings__raw_iter_get_current_record_pos(&file->iter);
    // if the callback returns false, stop iterating.
    if (!cb(file, &info, context)) {
      break;
    }
    pbl_settings__raw_iter_set_current_record_pos(&file->iter, file->cur_record_pos);
  }

  file->cur_record_pos = 0;
}

typedef struct {
  pbl_settings_rewrite_cb_t cb;
  struct pbl_settings_file *new_file;
  void *user_context;
} RewriteCbContext;
static bool prv_rewrite_cb(struct pbl_settings_file *file, struct pbl_settings_record *info,
                           void *context) {
  RewriteCbContext *cb_ctx = (RewriteCbContext *)context;
  cb_ctx->cb(file, cb_ctx->new_file, info, cb_ctx->user_context);
  return true; // continue iterating
}
int pbl_settings_rewrite(struct pbl_settings_file *file, pbl_settings_rewrite_cb_t cb,
                         void *context) {
  char *name = pbl_strdup(file->name);
  if (!name) {
    PBL_LOG_ERR("Could not allocate name to rewrite settings file %s", file->name);
    return -ENOMEM;
  }
  struct pbl_settings_file new_file;
  int ret = prv_open(&new_file, file->name, PBL_FS_O_REPLACE | PBL_FS_O_READ, file->max_used_space,
                     file->alloc_used_space, file->min_alloc_used_space);
  if (ret < 0) {
    pbl_free(name);
    return ret;
  }
  RewriteCbContext cb_ctx = (RewriteCbContext){
    .cb = cb,
    .new_file = &new_file,
    .user_context = context,
  };
  pbl_settings_each(file, prv_rewrite_cb, &cb_ctx);
  pbl_settings_close(file);
  // We have to close and reopen the new_file so that it's temp flag is cleared.
  // Before the close succeeds, if we reboot, we will just end up reading the
  // old file. After the close succeeds, we will end up reading the new
  // (compacted) file.
  int alloc_used_space = new_file.alloc_used_space;
  int min_alloc_used_space = new_file.min_alloc_used_space;
  pbl_settings_close(&new_file);
  ret = prv_open(file, name, PBL_FS_O_RDWR | PBL_FS_O_CREATE, file->max_used_space,
                 alloc_used_space, min_alloc_used_space);
  pbl_free(name);

  return ret;
}
