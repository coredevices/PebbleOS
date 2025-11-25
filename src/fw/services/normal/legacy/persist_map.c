/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "persist_map.h"

#include <stdio.h>
#include <stdlib.h>

#include "kernel/pbl_malloc.h"
#include "services/normal/filesystem/pfs.h"
#include "system/hexdump.h"
#include "system/logging.h"
#include "system/testinfra.h"
#include "util/attributes.h"
#include "util/math.h"

typedef struct PACKED {
  uint16_t version;
} PersistMapHeader;

#define EOF_PERSIST_ID_TAG ((int)(~0))
typedef struct PACKED {
  int id;
  Uuid uuid;
} PersistMapIdField;

typedef bool (*SearchCallback)(PersistMapIdField *field, void *data);

static const uint16_t PERSIST_MAP_VERSION = 1;

// Start off with a file which can hold 256 UUIDs
#define PMAP_FILE_SIZE \
  ((sizeof(PersistMapHeader) + sizeof(PersistMapIdField)) * 256)

static const char *s_map_filename = "pmap";

#if IS_BIGBOARD
#include "system/hexdump.h"
#include "system/testinfra.h"
#define NUM_SUCCESSFUL_OPENS_TO_TRACK 2
#define DIAGNOSTIC_ENTRY_SIZE 40
static uint8_t s_pmap_fd_diagnostic_data[NUM_SUCCESSFUL_OPENS_TO_TRACK][DIAGNOSTIC_ENTRY_SIZE];
static int s_next_pmap_fd_idx = 0;
#endif

// TODO: Remove once we figure out PBL-20973
static void prv_pmap_grab_debug_fd_data(int fd) {
#if IS_BIGBOARD
    extern void pfs_collect_diagnostic_data(int fd, void *diagnostic_buf,
                                            size_t diagnostic_buf_len);
    pfs_collect_diagnostic_data(fd, &s_pmap_fd_diagnostic_data[s_next_pmap_fd_idx],
                                DIAGNOSTIC_ENTRY_SIZE);

    s_next_pmap_fd_idx = (s_next_pmap_fd_idx + 1) % NUM_SUCCESSFUL_OPENS_TO_TRACK;
#endif
}

static int prv_pmap_open_debug_wrapper(const char *name, uint8_t op_flags, uint8_t file_type,
                                       size_t start_size) {
  int fd = pfs_open(name, op_flags, file_type, start_size);

#if IS_BIGBOARD
  if (fd < 0) { // pmap, where'd you go, we miss you so?!
    PBL_HEXDUMP_D(LOG_LEVEL_INFO, LOG_LEVEL_INFO, (uint8_t *)s_pmap_fd_diagnostic_data,
                  sizeof(s_pmap_fd_diagnostic_data));

    test_infra_quarantine_board("pmap file went missing");
  } else {
    prv_pmap_grab_debug_fd_data(fd);
  }
#endif /* IS_BIGBOARD */

  return fd;
}

static int seek_map(int fd, SearchCallback callback, void *data) {
  pfs_seek(fd, sizeof(PersistMapHeader), FSeekSet);

  bool found = false;
  PersistMapIdField field;
  while (true) {
    int read_result = pfs_read(fd, (uint8_t *)&field, sizeof(field));

    // short read, EOF, or check field to see if it matches
    // what's being searched for
    if ((read_result < (int)sizeof(field))) {
      if (PASSED(read_result) || read_result == E_RANGE) {
        return E_DOES_NOT_EXIST;
      }
      PBL_LOG(LOG_LEVEL_WARNING, "seek_map failed: %d", read_result);
      return read_result;
    } else  if (field.id == EOF_PERSIST_ID_TAG) {
      break;
    } else if ((found = callback(&field, data))) {
      pfs_seek(fd, -(int)sizeof(field), FSeekCur); // unwind to entry found
      break;
    }
  }

  return ((found) ? S_SUCCESS : E_DOES_NOT_EXIST);
}

static int search_map(SearchCallback callback, void *data) {
  int fd = prv_pmap_open_debug_wrapper(s_map_filename, OP_FLAG_READ, 0, 0);
  if (fd < 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "pmap search (open) failed: %d", fd);
    return (fd);
  }

  int seek_result = seek_map(fd, callback, data);

  pfs_close(fd);
  return (seek_result);
}

static bool walk_max_used_id(PersistMapIdField *field, void *data) {
  int *max_used_id = data;
  if (field->id > *max_used_id) {
    *max_used_id = field->id;
  }
  return false;
}

static int get_next_id() {
  int max_used_id = -1;
  search_map(walk_max_used_id, &max_used_id);
  return (max_used_id + 1);
}

// grows the size of the pmap file to the new_size specified
static status_t enlarge_pmap_file(int *fd, size_t new_size) {
  const size_t hunk_size = 256;
  uint8_t *buf = kernel_malloc(hunk_size);
  if (buf == NULL) {
    return (E_OUT_OF_MEMORY);
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "Growing pmap to %d bytes", (int)new_size);
  int new_fd = prv_pmap_open_debug_wrapper(s_map_filename, OP_FLAG_OVERWRITE, FILE_TYPE_STATIC,
      new_size);
  if (new_fd < 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "pmap enlarge (overwrite) failed: %d", new_fd);
    kernel_free(buf);
    return (new_fd);
  }

  size_t curr_file_len = pfs_get_file_size(*fd);
  int rv = S_SUCCESS;
  while (curr_file_len != 0) {
    size_t bytes_to_read = MIN(curr_file_len, hunk_size);

    if ((rv = pfs_read(*fd, buf, bytes_to_read)) != (int)bytes_to_read) {
      rv = (rv > 0) ? E_INTERNAL : rv;
      break;
    }
    if ((rv = pfs_write(new_fd, buf, bytes_to_read)) != (int)bytes_to_read) {
      rv = (rv > 0) ? E_INTERNAL : rv;
      break;
    }
    curr_file_len -= bytes_to_read;
  }

  pfs_close(*fd);
  pfs_close(new_fd);

  *fd = prv_pmap_open_debug_wrapper(s_map_filename, OP_FLAG_READ | OP_FLAG_WRITE, 0, 0);
  if (*fd < 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "pmap enlarge (re-open) failed: %d", *fd);
  }
  kernel_free(buf);
  return (rv);
}

int persist_map_add_uuid(const Uuid *uuid) {
  int id = get_next_id();

  int fd = prv_pmap_open_debug_wrapper(s_map_filename, OP_FLAG_READ | OP_FLAG_WRITE,
      FILE_TYPE_STATIC, 0);
  if (fd < 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "pmap add uuid (open) failed: %d", fd);
    return (fd);
  }

  int end_offset = (id * sizeof(PersistMapIdField)) + sizeof(PersistMapHeader);

  size_t file_sz = pfs_get_file_size(fd);
  if ((end_offset + sizeof(PersistMapIdField)) > file_sz) {
    status_t rv;
    file_sz = ((file_sz / PMAP_FILE_SIZE) + 1) * PMAP_FILE_SIZE;

    // TODO: The file is most likely corrupted in this situation. However, we
    // cannot simply remove the file because then when the file is recreated,
    // the wrong apps might map to certain persist files. We should migrate
    // pmap over to settings and as part of the migration we can sanity check
    // that nothing is corrupted. Today, when this happens, the best action a
    // user can take is to factory-reset
    if (file_sz > (PMAP_FILE_SIZE * 3)) {
      pfs_close(fd);
      PBL_LOG(LOG_LEVEL_WARNING, "pmap file is larger than expected, 0x%x 0x%x",
          (int)file_sz, (int)end_offset);
      return (E_INTERNAL);
    }

    if ((rv = enlarge_pmap_file(&fd, file_sz)) < 0) {
      pfs_close(fd);
      return (rv);
    }
  }

  int seek_to;
  if ((seek_to = pfs_seek(fd, end_offset, FSeekSet)) != end_offset) {
    PBL_LOG(LOG_LEVEL_WARNING, "Bad seek to %d, got %d", end_offset, seek_to);
    pfs_close(fd);
    return (seek_to);
  }

  PersistMapIdField field = {
    .id = id,
    .uuid = *uuid,
  };

  status_t append_status = pfs_write(fd, (uint8_t *)&field, sizeof(field));
  pfs_close(fd);

  return (FAILED(append_status) ? append_status : id);
}

static bool walk_get_id(PersistMapIdField *field, void *data) {
  PersistMapIdField *out_field = data;
  if (uuid_equal(&field->uuid, &out_field->uuid)) {
    out_field->id = field->id;
    return true;
  }
  return false;
}

int persist_map_get_id(const Uuid *uuid) {
  PersistMapIdField field = {
    .uuid = *uuid,
  };
  int search_result = search_map(walk_get_id, &field);
  if (FAILED(search_result)) {
    RETURN_STATUS_UP(search_result);
  }
  return field.id;
}

int persist_map_auto_id(const Uuid *uuid) {
  int id = persist_map_get_id(uuid);
  if (PASSED(id)) {
    return id;
  }

  if (id != E_DOES_NOT_EXIST) {
    RETURN_STATUS_UP(id);
  }

  return persist_map_add_uuid(uuid);
}

static bool walk_get_uuid(PersistMapIdField *field, void *data) {
  PersistMapIdField *out_field = data;
  if (field->id == out_field->id) {
    out_field->uuid = field->uuid;
    return true;
  }
  return false;
}

int persist_map_get_uuid(int id, Uuid *uuid) {
  PersistMapIdField field = {
    .id = id,
  };
  status_t search_result = search_map(walk_get_uuid, &field);
  if (PASSED(search_result)) {
    *uuid = field.uuid;
  }
  RETURN_STATUS(search_result);
}

static bool prv_dump_callback(PersistMapIdField *field, void *data) {
  char uuid_string[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(&field->uuid, uuid_string);

  PBL_LOG(LOG_LEVEL_INFO, "%s -> %d", uuid_string, field->id);

  return false;
}

void persist_map_dump(void) {
  PBL_LOG(LOG_LEVEL_INFO, "Dumping persist map:");
  search_map(prv_dump_callback, NULL);
}

status_t persist_map_init() {
  const char * const name = s_map_filename;
  status_t status = S_SUCCESS;

  // create a new map file if needed. We may fail on init so don't call it through the debug_wrapper
  int fd = pfs_open(name, OP_FLAG_READ, 0, 0);
  if (fd < 0) {
    if ((fd = prv_pmap_open_debug_wrapper(name, OP_FLAG_WRITE, FILE_TYPE_STATIC,
        PMAP_FILE_SIZE)) < 0) {
      PBL_LOG(LOG_LEVEL_WARNING, "pmap create failed: %d", fd);
      return (fd);
    }

    PersistMapHeader header = {
      .version = PERSIST_MAP_VERSION,
    };
    status = pfs_write(fd, (uint8_t *)&header, sizeof(header));
  } else {
    prv_pmap_grab_debug_fd_data(fd); // if the file exists, grab diagnostic data
  }

  pfs_close(fd);
  return (status);
}
