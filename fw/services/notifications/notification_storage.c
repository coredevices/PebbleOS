/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/services/notifications/notification_storage.h>
#include <pbl/services/notifications/notification_storage_private.h>
#include <pbl/services/timeline/attribute_private.h>
#include <pbl/util/iterator.h>
#include <pbl/util/math.h>
#include <pbl/util/uuid.h>

#include <kernel/pbl_malloc.h>

PBL_LOG_MODULE_DEFINE(service_notifications, CONFIG_SERVICE_NOTIFICATIONS_LOG_LEVEL);

typedef struct NotificationIterState {
  SerializedTimelineItemHeader header;
  struct pbl_fs_file *file;
  TimelineItem notification;
} NotificationIterState;

static const char *FILENAME = "/notifstr"; // The filename should not be changed

static PBL_MUTEX_DEFINE(s_notif_storage_mutex);

static uint32_t s_write_offset;

static bool prv_iter_next(IteratorState state);
static bool prv_get_notification(TimelineItem *notification, SerializedTimelineItemHeader *header,
                                 struct pbl_fs_file *file);
static void prv_set_header_status(SerializedTimelineItemHeader *header, uint8_t status,
                                  struct pbl_fs_file *file);

static int prv_open(struct pbl_fs_file *file, uint32_t flags) {
  int ret = pbl_fs_open(file, FILENAME, flags);
  if ((ret == 0) && ((flags & (PBL_FS_O_WRITE | PBL_FS_O_REPLACE)) != 0) &&
      (pbl_fs_size(file) == 0)) {
    ret = pbl_fs_truncate(file, NOTIFICATION_STORAGE_FILE_SIZE);
    if (ret < 0) {
      pbl_fs_close(file);
    }
  }

  return ret;
}

void notification_storage_init(void) {
  // Clear notifications storage on reset
  pbl_fs_unlink(FILENAME);
  // Create a new file and close it (removes delay when receiving first notification after boot)
  struct pbl_fs_file file;
  int ret = prv_open(&file, PBL_FS_O_WRITE | PBL_FS_O_CREATE);
  if (ret < 0) {
    PBL_LOG_ERR("Error opening file %d", ret);
  } else {
    pbl_fs_close(&file);
  }
  s_write_offset = 0;
}

void notification_storage_lock(void) {
  pbl_mutex_lock(&s_notif_storage_mutex, PBL_FOREVER);
}

void notification_storage_unlock(void) {
  pbl_mutex_unlock(&s_notif_storage_mutex);
}

static int prv_file_open(struct pbl_fs_file *file, uint32_t flags) {
  notification_storage_lock();
  int ret = prv_open(file, flags);
  if (ret < 0) {
    // If a write operation or a read operation fails with anything other than "does not exist",
    // log error
    bool read_only = ((flags & (PBL_FS_O_WRITE | PBL_FS_O_REPLACE)) == 0);
    if ((!read_only) || (ret != -ENOENT)) {
      PBL_LOG_ERR("Error opening file %d", ret);
      // Remove file so next open will create a new one (notification storage trashed)
      pbl_fs_unlink(FILENAME);
    }
    notification_storage_unlock();
  }
  return ret;
}

static void prv_file_close(struct pbl_fs_file *file) {
  pbl_fs_close(file);
  notification_storage_unlock();
}

static int prv_write_notification(TimelineItem *notification, SerializedTimelineItemHeader *header,
                                  struct pbl_fs_file *file) {
  int bytes_written = 0;

  // Invert flags & status to store on flash
  header->common.flags = ~header->common.flags;
  header->common.status = ~header->common.status;

  int result = pbl_fs_write(file, (uint8_t *)header, sizeof(*header));

  // Restore flags & status
  header->common.flags = ~header->common.flags;
  header->common.status = ~header->common.status;

  if (result < 0) {
    PBL_LOG_ERR("Error writing notification header %d", result);
    return result;
  }
  bytes_written += result;

  if (!header->payload_length) {
    return result;
  }

  uint8_t *write_buffer = kernel_malloc_check(header->payload_length);

  timeline_item_serialize_payload(notification, write_buffer, header->payload_length);
  result = pbl_fs_write(file, write_buffer, header->payload_length);
  if (result < 0) {
    PBL_LOG_ERR("Error writing notification payload %d", result);
    kernel_free(write_buffer);
    return result;
  }
  bytes_written += result;

  kernel_free(write_buffer);

  return bytes_written;
}

//! Iterate over notifications space and mark the oldest notifications as deleted until we have
//! enough space available
static void prv_reclaim_space(size_t size_needed, struct pbl_fs_file *file) {
  size_needed = ((size_needed / NOTIFICATION_STORAGE_MINIMUM_INCREMENT_SIZE) + 1) *
                NOTIFICATION_STORAGE_MINIMUM_INCREMENT_SIZE; // Free up space size in blocks
  size_t size_available = 0;
  NotificationIterState iter_state = {
    .file = file,
  };
  Iterator iter;
  iter_init(&iter, prv_iter_next, nullptr, &iter_state);
  while (iter_next(&iter)) {
    uint8_t status = iter_state.header.common.status;
    if (!(status & TimelineItemStatusDeleted)) {
      // Mark for deletion
      char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
      uuid_to_string(&iter_state.header.common.id, uuid_buffer);
      PBL_LOG_WRN("Storage full: marking notification %s as deleted (ANCS UID: %" PRIu32 ")",
                  uuid_buffer, iter_state.header.common.ancs_uid);
      prv_set_header_status(&iter_state.header, TimelineItemStatusDeleted, file);
      size_available += sizeof(SerializedTimelineItemHeader) + iter_state.header.payload_length;
      if (size_needed <= size_available) {
        return;
      }
    }
    if (pbl_fs_seek(file, iter_state.header.payload_length, PBL_FS_SEEK_CUR) < 0) {
      break;
    }
  }
}

//! Check whether there exists @c size_needed available space in storage after compression
static bool prv_is_storage_full(size_t size_needed, size_t *size_available,
                                struct pbl_fs_file *file) {
  *size_available = 0;
  NotificationIterState iter_state = {
    .file = file,
  };
  Iterator iter;
  iter_init(&iter, prv_iter_next, nullptr, &iter_state);
  while (iter_next(&iter)) {
    // Check header status to detect deleted notifications. Add size of deleted notifications
    uint8_t status = iter_state.header.common.status;
    if (status & TimelineItemStatusDeleted) {
      *size_available += sizeof(SerializedTimelineItemHeader) + iter_state.header.payload_length;
      if (size_needed <= *size_available) {
        return false;
      }
    }
    if (pbl_fs_seek(file, iter_state.header.payload_length, PBL_FS_SEEK_CUR) < 0) {
      break;
    }
  }
  return true;
}

//! Compress storage by reading all valid notifications out of old file and storing to new file
//! via overwrite
static bool prv_compress(size_t size_needed, struct pbl_fs_file *file) {
  pbl_fs_seek(file, 0, PBL_FS_SEEK_SET);

  // Open file for overwrite
  struct pbl_fs_file new_file;
  int ret = prv_open(&new_file, PBL_FS_O_REPLACE);
  if (ret < 0) {
    PBL_LOG_ERR("Error opening new file for compression %d", ret);
    return false;
  }

  // Delete old notifications if there is no space left in storage
  size_t size_available;
  if (prv_is_storage_full(size_needed, &size_available, file)) {
    pbl_fs_seek(file, 0, PBL_FS_SEEK_SET);
    prv_reclaim_space(size_needed - size_available, file);
  }
  pbl_fs_seek(file, 0, PBL_FS_SEEK_SET);

  int write_offset = 0;

  // Iterate over notifications stored and write to new file
  NotificationIterState iter_state = {
    .file = file,
  };
  Iterator iter;
  iter_init(&iter, prv_iter_next, nullptr, &iter_state);
  while (iter_next(&iter)) {
    // Check header flags to detect deleted notifications
    uint8_t status = iter_state.header.common.status;
    if (status & TimelineItemStatusDeleted) {
      // Skip over deleted notification
      pbl_fs_seek(file, iter_state.header.payload_length, PBL_FS_SEEK_CUR);
      continue;
    }

    TimelineItem notification;
    if (!prv_get_notification(&notification, &iter_state.header, file)) {
      // Error occurred
      char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
      uuid_to_string(&iter_state.header.common.id, uuid_buffer);
      PBL_LOG_ERR("Failed to read notification %s during compression. Resetting all notifications.",
                  uuid_buffer);
      goto cleanup;
    }
    int result = prv_write_notification(&notification, &iter_state.header, &new_file);
    if (result < 0) {
      // Error occurred
      char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
      uuid_to_string(&iter_state.header.common.id, uuid_buffer);
      PBL_LOG_ERR(
          "Failed to write notification %s during compression (error %d). Resetting all notifications.",
          uuid_buffer, result);
      // The buffer comes from the calling task's heap (timeline_item_deserialize_item), so it
      // must not be freed with kernel_free: compression can run on the app task, e.g. when a
      // workout summary notification is stored while storage is full.
      timeline_item_free_allocated_buffer(&notification);
      goto cleanup;
    }
    write_offset += result;
    timeline_item_free_allocated_buffer(&notification);
  }

  s_write_offset = write_offset;

  pbl_fs_close(file);
  pbl_fs_close(&new_file);

  ret = prv_open(file, PBL_FS_O_RDWR | PBL_FS_O_CREATE);
  if (ret < 0) {
    PBL_LOG_ERR("Error re-opening after compression %d", ret);
    return false;
  }

  return true;

cleanup:
  pbl_fs_close(file);
  pbl_fs_close(&new_file);
  return false;
}

void notification_storage_store(TimelineItem *notification) {
  PBL_ASSERTN(notification != nullptr);

  SerializedTimelineItemHeader header = {.common.id = UUID_INVALID};
  timeline_item_serialize_header(notification, &header);

  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_RDWR | PBL_FS_O_CREATE) < 0) {
    return;
  }
  size_t size_needed = header.payload_length + sizeof(SerializedTimelineItemHeader);
  if (size_needed > (NOTIFICATION_STORAGE_FILE_SIZE - s_write_offset)) {
    if (!prv_compress(size_needed, &file)) {
      // Notification storage compression failed. Clear notifications storage
      PBL_LOG_ERR("Notification storage compression failed! Resetting all notifications.");
      goto reset_storage;
    }
  }

  pbl_fs_seek(&file, s_write_offset, PBL_FS_SEEK_SET);

  int result = prv_write_notification(notification, &header, &file);
  if (result < 0) {
    // [AS] TODO: Write failure: reset storage, compression or reset watch?
    char uuid_buffer[UUID_STRING_BUFFER_LENGTH];
    uuid_to_string(&notification->header.id, uuid_buffer);
    PBL_LOG_ERR("Failed to write notification %s (error %d). Resetting all notifications.",
                uuid_buffer, result);
    goto reset_storage;
  }

  s_write_offset += result;

  prv_file_close(&file);
  return;

reset_storage:
  pbl_mutex_unlock(&s_notif_storage_mutex);
  notification_storage_reset_and_init();
}

// Finds the next match in the notification storage file from the current position
// Position in file will be at the start of notification payload if return value is true
static bool prv_find_next_notification(SerializedTimelineItemHeader *header,
                                       bool (*compare_func)(SerializedTimelineItemHeader *header,
                                                            void *data),
                                       void *data, struct pbl_fs_file *file) {
  for (;;) {
    int result = pbl_fs_read(file, (uint8_t *)header, sizeof(*header));

    // Restore flags & status
    header->common.flags = ~header->common.flags;
    header->common.status = ~header->common.status;

    if ((result != (int)sizeof(*header)) || (uuid_is_invalid(&header->common.id))) {
      break;
    }

    uint8_t status = header->common.status;
    if ((status & TimelineItemStatusUnused) ||
        (header->common.type >= TimelineItemTypeOutOfRange) ||
        (header->common.layout >= NumLayoutIds)) {
      pbl_fs_close(file);
      notification_storage_reset_and_init();
      PBL_LOG_ERR("Notification storage corrupt. Resetting...");
      break;
    }

    if (!(status & TimelineItemStatusDeleted)) {
      // Only check compare notifications if it is not deleted, otherwise skip it and look for
      // the next match
      if (compare_func(header, data)) {
        return true;
      }
    }

    if (pbl_fs_seek(file, header->payload_length, PBL_FS_SEEK_CUR) < 0) {
      break;
    }
  }

  return false;
}

static bool prv_uuid_equal_func(SerializedTimelineItemHeader *header, void *data) {
  Uuid *uuid = (Uuid *)data;
  return uuid_equal(&header->common.id, uuid);
}

static bool prv_ancs_id_compare_func(SerializedTimelineItemHeader *header, void *data) {
  uintptr_t ancs_uid = (uintptr_t)data;
  return header->common.ancs_uid == ancs_uid;
}

bool notification_storage_notification_exists(const Uuid *id) {
  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_READ) < 0) {
    return false;
  }

  SerializedTimelineItemHeader header = {.common.id = UUID_INVALID};
  bool found = prv_find_next_notification(&header, prv_uuid_equal_func, (void *)id, &file);

  prv_file_close(&file);

  return found;
}

static bool prv_get_notification(TimelineItem *notification, SerializedTimelineItemHeader *header,
                                 struct pbl_fs_file *file) {
  notification->allocated_buffer = nullptr; // Must be initialized in case this goes to cleanup
  // Read notification to temporary buffer
  uint8_t *read_buffer = task_zalloc_check(header->payload_length);

  int result = pbl_fs_read(file, read_buffer, header->payload_length);
  if (result != (int)header->payload_length) {
    goto cleanup;
  }

  if (!timeline_item_deserialize_item(notification, header, read_buffer)) {
    goto cleanup;
  }

  task_free(read_buffer);
  return true;

cleanup:
  task_free(read_buffer);
  return false;
}

size_t notification_storage_get_len(const Uuid *uuid) {
  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_READ) < 0) {
    return 0;
  }

  size_t size = 0;
  SerializedTimelineItemHeader header = {.common.id = UUID_INVALID};
  if (prv_find_next_notification(&header, prv_uuid_equal_func, (void *)uuid, &file)) {
    size = header.payload_length + sizeof(SerializedTimelineItemHeader);
  } else {
    PBL_LOG_DBG("notification not found");
  }

  prv_file_close(&file);
  return (size);
}

bool notification_storage_get(const Uuid *id, TimelineItem *item_out) {
  PBL_ASSERTN(item_out);

  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_READ) < 0) {
    return false;
  }

  bool rv = true;

  SerializedTimelineItemHeader header = {.common.id = UUID_INVALID};
  char uuid_string[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(id, uuid_string);
  if (!prv_find_next_notification(&header, prv_uuid_equal_func, (void *)id, &file)) {
    PBL_LOG_DBG("notification not found, %s", uuid_string);
    rv = false;
  } else {
    if (!prv_get_notification(item_out, &header, &file)) {
      PBL_LOG_ERR("Could not retrieve notification with id %s and size %u", uuid_string,
                  header.payload_length);
      rv = false;
    }
  }

  prv_file_close(&file);

  return rv;
}

//! @return is_advanced
static bool prv_iter_next(IteratorState state) {
  NotificationIterState *iter_state = state;
  int result =
      pbl_fs_read(iter_state->file, (uint8_t *)&iter_state->header, sizeof(iter_state->header));

  // Restore flags & status
  iter_state->header.common.flags = ~iter_state->header.common.flags;
  iter_state->header.common.status = ~iter_state->header.common.status;

  if (((result >= 0) && (result != (int)sizeof(iter_state->header))) ||
      (uuid_is_invalid(&iter_state->header.common.id))) {
    // End iteration if we have reached the end of the file or the header ID is invalid (erased
    // flash)
    return false;
  } else if (result < 0) {
    PBL_LOG_ERR("Error reading notification header while iterating %d", result);
    return false;
  }

  return true;
}

static bool prv_rewrite_iter_next(IteratorState state) {
  NotificationIterState *iter_state = state;
  int result = 0;

  result =
      pbl_fs_read(iter_state->file, (uint8_t *)&iter_state->header, sizeof(iter_state->header));

  // Restore flags & status
  iter_state->header.common.flags = ~iter_state->header.common.flags;
  iter_state->header.common.status = ~iter_state->header.common.status;

  if (((result >= 0) && (result != (int)sizeof(iter_state->header))) ||
      (uuid_is_invalid(&iter_state->header.common.id))) {
    return false;
  } else if (result < 0) {
    PBL_LOG_ERR("Error reading notification header while iterating %d", result);
    return false;
  }

  if (iter_state->header.common.status & TimelineItemStatusDeleted) {
    // Deleted entries are not read into iter_state->notification; skip the payload so the next
    // iteration reads a record header, not payload bytes
    return pbl_fs_seek(iter_state->file, iter_state->header.payload_length, PBL_FS_SEEK_CUR) >= 0;
  }
  return prv_get_notification(&iter_state->notification, &iter_state->header, iter_state->file);
}

static void prv_set_header_status(SerializedTimelineItemHeader *header, uint8_t status,
                                  struct pbl_fs_file *file) {
  // Seek to the status field
  pbl_fs_seek(file, (-(int)sizeof(*header) + (int)offsetof(CommonTimelineItemHeader, status)),
              PBL_FS_SEEK_CUR);

  // Invert flags & status to store on flash
  status = ~status;

  int result = pbl_fs_write(file, (uint8_t *)&status, sizeof(header->common.status));
  if (result < 0) {
    PBL_LOG_ERR("Error writing status to notification header %d", result);
  }

  // Seek to the end of the header
  pbl_fs_seek(file,
              ((int)sizeof(*header) - (int)offsetof(CommonTimelineItemHeader, status) -
               sizeof(header->common.status)),
              PBL_FS_SEEK_CUR);
}

bool notification_storage_get_status(const Uuid *id, uint8_t *status) {
  struct pbl_fs_file file;
  bool rv = false;
  if (prv_file_open(&file, PBL_FS_O_READ) < 0) {
    return rv;
  }

  SerializedTimelineItemHeader header = {.common.id = UUID_INVALID};
  if (prv_find_next_notification(&header, prv_uuid_equal_func, (void *)id, &file)) {
    *status = header.common.status;
    rv = true;
  }

  prv_file_close(&file);
  return rv;
}

void notification_storage_set_status(const Uuid *id, uint8_t status) {
  SerializedTimelineItemHeader header = {.common.id = UUID_INVALID};

  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_RDWR | PBL_FS_O_CREATE) < 0) {
    return;
  }

  if (prv_find_next_notification(&header, prv_uuid_equal_func, (void *)id, &file)) {
    prv_set_header_status(&header, status, &file);
  }

  prv_file_close(&file);
}

void notification_storage_remove(const Uuid *id) {
  notification_storage_set_status(id, TimelineItemStatusDeleted);
}

bool notification_storage_find_ancs_notification_id(uint32_t ancs_uid, Uuid *uuid_out) {
  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_READ) < 0) {
    return false;
  }

  SerializedTimelineItemHeader header = {.common.id = UUID_INVALID};

  // Find the most recent notification which matches this ANCS UID - this will be the last entry in
  // the db. iOS can reset ANCS UIDs on reconnect, so we want to avoid finding an old notification
  bool found = false;
  while (prv_find_next_notification(&header, prv_ancs_id_compare_func, (void *)(uintptr_t)ancs_uid,
                                    &file)) {
    found = true;
    *uuid_out = header.common.id;

    // Seek to the end of this item's payload (start of the next item)
    if (pbl_fs_seek(&file, header.payload_length, PBL_FS_SEEK_CUR) < 0) {
      break;
    }
  }

  prv_file_close(&file);

  return found;
}

static bool prv_compare_ancs_notifications(TimelineItem *notification, const uint8_t *payload,
                                           size_t payload_size,
                                           SerializedTimelineItemHeader *header,
                                           struct pbl_fs_file *file) {
  if ((notification->header.timestamp != header->common.timestamp) ||
      (notification->header.layout != header->common.layout) ||
      (header->payload_length != payload_size)) {
    return false;
  }

  bool found = false;
  if (header->payload_length == payload_size) {
    uint8_t *read_buffer = kernel_malloc_check(payload_size);

    int result = pbl_fs_read(file, read_buffer, payload_size);
    if (result != (int)payload_size) {
      kernel_free(read_buffer);
      return false;
    }

    // Seek back to the end of the header so that the next iterator seek finds the next record
    pbl_fs_seek(file, -(off_t)payload_size, PBL_FS_SEEK_CUR);

    found = (memcmp(payload, read_buffer, payload_size) == 0);
    kernel_free(read_buffer);
  }
  return found;
}

bool notification_storage_find_ancs_notification_by_timestamp(
    TimelineItem *notification, CommonTimelineItemHeader *header_out) {
  PBL_ASSERTN(notification && header_out);

  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_READ) < 0) {
    return false;
  }

  // Serialize notification attributes and actions for easy comparison
  size_t payload_size = timeline_item_get_serialized_payload_size(notification);
  uint8_t *payload = kernel_malloc_check(payload_size);
  timeline_item_serialize_payload(notification, payload, payload_size);

  // Iterate over all records until a match is found
  bool rv = false;
  NotificationIterState iter_state = {
    .file = &file,
  };
  Iterator iter;
  iter_init(&iter, prv_iter_next, nullptr, &iter_state);
  while (iter_next(&iter)) {
    // Check header flags to detect deleted notifications
    uint8_t status = iter_state.header.common.status;
    if (!(status & TimelineItemStatusDeleted)) {
      if (prv_compare_ancs_notifications(notification, payload, payload_size, &iter_state.header,
                                         &file)) {
        *header_out = iter_state.header.common;
        rv = true;
        break;
      }
    }
    int result = pbl_fs_seek(iter_state.file, iter_state.header.payload_length, PBL_FS_SEEK_CUR);
    if (result < 0) {
      break;
    }
  }

  kernel_free(payload);

  prv_file_close(&file);

  return rv;
}

void notification_storage_rewrite(void (*iter_callback)(TimelineItem *notification,
                                                        SerializedTimelineItemHeader *header,
                                                        void *data),
                                  void *data) {
  if (iter_callback == nullptr) {
    return;
  }

  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_RDWR | PBL_FS_O_CREATE) < 0) {
    return;
  }

  struct pbl_fs_file new_file;
  if (prv_open(&new_file, PBL_FS_O_REPLACE | PBL_FS_O_READ) < 0) {
    prv_file_close(&file);
    return;
  }

  Iterator iter;
  NotificationIterState iter_state = {.file = &file};
  iter_init(&iter, prv_rewrite_iter_next, nullptr, &iter_state);

  int write_offset = 0;
  while (iter_next(&iter)) {
    uint8_t status = iter_state.header.common.status;
    if (status & TimelineItemStatusDeleted) {
      // Drop deleted entries; iter_state.notification still holds the previous item, whose
      // buffer has already been written and freed
      continue;
    }
    iter_callback(&iter_state.notification, &iter_state.header, data);
    int result = prv_write_notification(&iter_state.notification, &iter_state.header, &new_file);
    timeline_item_free_allocated_buffer(&iter_state.notification);
    if (result < 0) {
      break;
    }
    write_offset += result;
  }
  s_write_offset = write_offset;

  // Close the old file
  prv_file_close(&file);

  // We have to close and reopened the new file, so that it's temp flag is cleared.
  pbl_fs_close(&new_file);
  if (prv_file_open(&new_file, PBL_FS_O_RDWR | PBL_FS_O_CREATE) == 0) {
    prv_file_close(&new_file);
  }
}

void notification_storage_iterate(bool (*iter_callback)(void *data,
                                                        SerializedTimelineItemHeader *header),
                                  void *data) {
  if (iter_callback == nullptr) {
    return;
  }

  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_READ) < 0) {
    return;
  }

  Iterator iter;
  NotificationIterState iter_state = {.file = &file};

  iter_init(&iter, prv_iter_next, nullptr, &iter_state);

  while (iter_next(&iter)) {
    uint8_t status = iter_state.header.common.status;
    if (!(status & TimelineItemStatusDeleted)) {
      if (!iter_callback(data, &iter_state.header)) {
        break;
      }
    }
    int result = pbl_fs_seek(iter_state.file, iter_state.header.payload_length, PBL_FS_SEEK_CUR);
    if (result < 0) {
      break;
    }
  }

  prv_file_close(&file);
}

//! Reads the string attributes requested in attr_list from the payload following the header,
//! leaving the file positioned after the payload
static bool prv_read_string_attributes(const SerializedTimelineItemHeader *header,
                                       AttributeList *attr_list, size_t buffer_size,
                                       struct pbl_fs_file *file) {
  for (uint8_t i = 0; i < attr_list->num_attributes; i++) {
    attr_list->attributes[i].cstring[0] = '\0';
  }

  uint32_t remaining = header->payload_length;
  for (uint8_t i = 0; i < header->num_attributes; i++) {
    SerializedAttributeHeader attribute;
    if ((remaining < sizeof(attribute)) ||
        (pbl_fs_read(file, (uint8_t *)&attribute, sizeof(attribute)) != sizeof(attribute))) {
      return false;
    }
    remaining -= sizeof(attribute);
    if (attribute.length > remaining) {
      return false;
    }
    remaining -= attribute.length;

    Attribute *dest = attribute_find(attr_list, attribute.id);
    const size_t length = dest ? MIN(attribute.length, buffer_size - 1) : 0;
    if ((length > 0) && (pbl_fs_read(file, (uint8_t *)dest->cstring, length) != (ssize_t)length)) {
      return false;
    }
    if (dest) {
      dest->cstring[length] = '\0';
    }
    if (pbl_fs_seek(file, attribute.length - length, PBL_FS_SEEK_CUR) < 0) {
      return false;
    }
  }

  return pbl_fs_seek(file, remaining, PBL_FS_SEEK_CUR) >= 0;
}

void notification_storage_iterate_strings_after(
    time_t item_cutoff, AttributeList *attr_list, size_t buffer_size,
    bool (*iter_callback)(void *data, const CommonTimelineItemHeader *header,
                          const TimelineItem *item),
    void *data) {
  if (iter_callback == nullptr) {
    return;
  }

  struct pbl_fs_file file;
  if (prv_file_open(&file, PBL_FS_O_READ) < 0) {
    return;
  }

  Iterator iter;
  NotificationIterState iter_state = {.file = &file};
  iter_init(&iter, prv_iter_next, nullptr, &iter_state);

  while (iter_next(&iter)) {
    const uint8_t status = iter_state.header.common.status;
    const bool corrupt = (status & TimelineItemStatusUnused) ||
                         (iter_state.header.common.type >= TimelineItemTypeOutOfRange) ||
                         (iter_state.header.common.layout >= NumLayoutIds);
    if (corrupt) {
      PBL_LOG_WRN("Skipping corrupt notification");
    }
    if (corrupt || (status & TimelineItemStatusDeleted)) {
      if (pbl_fs_seek(&file, iter_state.header.payload_length, PBL_FS_SEEK_CUR) < 0) {
        break;
      }
      continue;
    }

    const bool read_strings = iter_state.header.common.timestamp >= item_cutoff;
    if (read_strings) {
      const int payload_offset = pbl_fs_seek(&file, 0, PBL_FS_SEEK_CUR);
      if (payload_offset < 0) {
        break;
      }
      if (!prv_read_string_attributes(&iter_state.header, attr_list, buffer_size, &file)) {
        PBL_LOG_WRN("Skipping corrupt notification payload");
        if (pbl_fs_seek(&file, payload_offset + iter_state.header.payload_length, PBL_FS_SEEK_SET) <
            0) {
          break;
        }
        continue;
      }
    } else if (pbl_fs_seek(&file, iter_state.header.payload_length, PBL_FS_SEEK_CUR) < 0) {
      break;
    }

    const TimelineItem item = {
      .header = iter_state.header.common,
      .attr_list = *attr_list,
    };
    if (!iter_callback(data, &iter_state.header.common, read_strings ? &item : nullptr)) {
      break;
    }
  }

  prv_file_close(&file);
}

void notification_storage_reset_and_init(void) {
  notification_storage_lock();
  pbl_fs_unlink(FILENAME);
  s_write_offset = 0;
  notification_storage_unlock();
}

// Added for use by unit tests. Do not call from firmware
#if UNITTEST
void notification_storage_reset(void) {
  notification_storage_init();
}
#endif
