/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "get_bytes_storage_file.h"
#include "get_bytes_private.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "kernel/core_dump.h"
#include "kernel/pbl_malloc.h"
#include "system/logging.h"
#include "system/status_codes.h"

typedef struct {
  uint32_t core_dump_base;
  bool only_get_new_coredump;
} GBCoredumpData;

// -----------------------------------------------------------------------------------------------
// Search the possible locations for a core dump image in flash and return flash base address of the
// most recently written one.
// @param unread_only only consider coredump slots that have not been read when searching
// @return flash base address of most core dump image, or CORE_DUMP_FLASH_INVALID_ADDR if none found
static uint32_t prv_coredump_flash_base(bool unread_only) {
  CoreDumpFlashHeader flash_hdr;
  CoreDumpFlashRegionHeader region_hdr;
  uint32_t max_last_used = 0;
  uint32_t  base_address;
  uint32_t last_used_idx = 0;

  // ----------------------------------------------------------------------------------
  // First, see if the flash header has been put in place
  flash_read_bytes((uint8_t *)&flash_hdr, CORE_DUMP_FLASH_START, sizeof(flash_hdr));

  if (flash_hdr.magic != CORE_DUMP_FLASH_HDR_MAGIC
        || flash_hdr.unformatted == CORE_DUMP_ALL_UNFORMATTED) {
    return CORE_DUMP_FLASH_INVALID_ADDR;
  }

  // Find the region with the highest last_used count
  for (unsigned int i=0; i<CORE_DUMP_MAX_IMAGES; i++) {
    if (flash_hdr.unformatted & (1 << i)) {
      continue;
    }

    base_address = core_dump_get_slot_address(i);
    flash_read_bytes((uint8_t *)&region_hdr, base_address, sizeof(region_hdr));

    if (unread_only && !region_hdr.unread) {
      continue;
    }

    if (region_hdr.last_used > max_last_used) {
      max_last_used = region_hdr.last_used;
      last_used_idx = i;
    }
  }

  if (!max_last_used) {
    return CORE_DUMP_FLASH_INVALID_ADDR;
  }

  return core_dump_get_slot_address(last_used_idx);
}

bool gb_storage_coredump_setup(GetBytesStorage *storage, GetBytesObjectType object_type,
                            GetBytesStorageInfo *info) {
  storage->impl_data = kernel_zalloc_check(sizeof(GBCoredumpData));
  ((GBCoredumpData *)storage->impl_data)->only_get_new_coredump = info->only_get_new_coredump;
  return true;
}

GetBytesInfoErrorCode gb_storage_coredump_get_size(GetBytesStorage *storage, uint32_t *size) {
  GBCoredumpData *data = storage->impl_data;
  CoreDumpImageHeader image_hdr;

  // Get the base address in flash
  uint32_t flash_base = prv_coredump_flash_base(data->only_get_new_coredump);
  PBL_LOG(LOG_LEVEL_DEBUG, "GET_BYTES: checking image %p", (void *)flash_base);
  if (flash_base != CORE_DUMP_FLASH_INVALID_ADDR) {
    flash_read_bytes((uint8_t *)&image_hdr, flash_base + sizeof(CoreDumpFlashRegionHeader),
                     sizeof(image_hdr));
  }
  if (flash_base == CORE_DUMP_FLASH_INVALID_ADDR || image_hdr.magic != CORE_DUMP_MAGIC) {
    return GET_BYTES_DOESNT_EXIST;
  }

  status_t err = core_dump_size(flash_base, size);
  if (err != S_SUCCESS) {
    return GET_BYTES_CORRUPTED;
  } else {
    data->core_dump_base = flash_base;
    return GET_BYTES_OK;
  }
}

bool gb_storage_coredump_read_next_chunk(GetBytesStorage *storage, uint8_t *buffer, uint32_t len) {
  GBCoredumpData *data = storage->impl_data;
  uint32_t image_base = data->core_dump_base + sizeof(CoreDumpFlashRegionHeader);
  flash_read_bytes(buffer, image_base + storage->current_offset, len);
  storage->current_offset += len;
  return true;
}

void gb_storage_coredump_cleanup(GetBytesStorage *storage, bool successful) {
  // if successful, mark the coredump as read
  if (successful) {
    GBCoredumpData *data = storage->impl_data;
    core_dump_mark_read(data->core_dump_base);
  }

  kernel_free(storage->impl_data);
}

bool is_unread_coredump_available(void) {
  uint32_t flash_base = prv_coredump_flash_base(true);

  return core_dump_is_unread_available(flash_base);
}
