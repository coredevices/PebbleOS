/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/blob_db/sync_util.h>

PBL_LOG_MODULE_DECLARE(service_blob_db, CONFIG_SERVICE_BLOB_DB_LOG_LEVEL);

// Caution: CommonTimelineItemHeader .flags & .status are stored inverted and not auto-restored
// by the underlying db API. If .flags or .status is used from a CommonTimelineItemHeader below,
// be very careful

bool sync_util_is_dirty_cb(struct pbl_settings_file *file, struct pbl_settings_record *info,
                           void *context) {
  // If there is a single dirty record, update the out bool to dirty and stop iterating
  if (info->dirty) {
    *((bool *)context) = true;
    return false;
  }

  return true;
}

bool sync_util_build_dirty_list_cb(struct pbl_settings_file *file, struct pbl_settings_record *info,
                                   void *context) {
  if (info->dirty) {
    BlobDBDirtyItem *dirty_list = *(BlobDBDirtyItem **)context;

    BlobDBDirtyItem *new_node = pbl_zalloc(sizeof(BlobDBDirtyItem) + info->key_len);
    if (!new_node) {
      PBL_LOG_WRN("Ran out of memory while building a dirty list");
      return false;
    }

    new_node->last_updated = info->last_modified;
    new_node->key_len = info->key_len;
    pbl_settings_read_key(file, new_node->key, new_node->key_len);

    *(BlobDBDirtyItem **)context =
        (BlobDBDirtyItem *)list_prepend((ListNode *)dirty_list, (ListNode *)new_node);
  }

  return true;
}
