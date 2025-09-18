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

#include "put_bytes_storage_raw.h"

#include "drivers/flash.h"
#include "drivers/task_watchdog.h"
#include "flash_region/flash_region.h"
#include "kernel/pbl_malloc.h"
#include "resource/resource_storage_flash.h"
#include "system/firmware_storage.h"
#include "util/math.h"

typedef struct MemoryLayout {
  //! The start address of the object's section in flash (inclusive)
  uint32_t start_address;

  //! The end address of the object's section in flash (exclusive)
  uint32_t end_address;

  //! An optional offset from the beginning of the object's section in flash. This is useful if
  //! you need to insert some derived metadata after the object has been written to flash.
  uint32_t start_offset;
} MemoryLayout;

static const MemoryLayout* prv_get_layout_for_type(PutBytesObjectType object_type) {
  static const MemoryLayout layouts[] = {
    { FLASH_REGION_FIRMWARE_DEST_BEGIN,
      FLASH_REGION_FIRMWARE_DEST_END, sizeof(FirmwareDescription) },
    { FLASH_REGION_FIRMWARE_DEST_BEGIN,
      FLASH_REGION_FIRMWARE_DEST_END, sizeof(FirmwareDescription) },
  };
  static MemoryLayout resource_layout;

  // Our PutBytesObjectType values are 1 indexed instead of zero indexed and the first three
  // are the only raw ones, so we can get away with just subtracting 1.
  if (object_type == ObjectSysResources) {
    resource_layout = (MemoryLayout) {
        resource_storage_flash_get_unused_bank()->begin,
        resource_storage_flash_get_unused_bank()->end,
        0,
    };
    return &resource_layout;
  } else {
    return &layouts[object_type - 1];
  }
}

bool pb_storage_raw_get_status(PutBytesObjectType obj_type,  PbInstallStatus *status) {
  const MemoryLayout *layout  = prv_get_layout_for_type(obj_type);

  const size_t read_buffer_size = 2048;
  uint8_t *read_buffer = kernel_zalloc(read_buffer_size);

  bool success = false;

  if (!read_buffer) {
    // If we can't alloc 2kB put bytes is likely going to fail anyway so just fallback to default
    // impl
    return success;
  }

  uint32_t curr_read_address = layout->end_address;
  uint32_t stop_read_address = layout->start_address + layout->start_offset;

  // Walk through the bank backwards. We rely on the NOR flash property that an "erased" byte will
  // read 0xff. When we encounter a byte which is not 0xff, we have found data which has been
  // written.
  while (curr_read_address > stop_read_address) {
    size_t bytes_left = curr_read_address - stop_read_address;
    size_t bytes_to_read = MIN(bytes_left, read_buffer_size);
    curr_read_address -= bytes_to_read;

    flash_read_bytes(read_buffer, curr_read_address, bytes_to_read);
    for (int i = (bytes_to_read - 1); i >= 0; i--) {
      if (read_buffer[i] != 0xff) {
        uint32_t data_at = curr_read_address + i;

        // FIXME: To get bytes_written we should really be adding + 1. However, for FW installs,
        // PBs expects that a resource pack and firmware has been transmitted. To guarantee this
        // happens, never tell the mobile app that all bytes have been transferred. I don't see a
        // great way of resolving this that doesn't result in messing around with the PB state
        // machine which I'd like to avoid if this gets pulled into silk PRF
        uint32_t bytes_written = data_at - stop_read_address;
        if (bytes_written == 0) {
          goto cleanup;
        }

        // TODO: We are perpetuating the defective crc here. Maybe this is as good an excuse as any
        // for the mobile apps to implement flash_crc32
        uint32_t crc = flash_calculate_legacy_defective_checksum(stop_read_address, bytes_written);

        *status = (PbInstallStatus) {
          .num_bytes_written = bytes_written,
          .crc_of_bytes = crc
        };

        success = true;
        goto cleanup;
      }
    }
  }

cleanup:
  kernel_free(read_buffer);
  return success;
}

bool pb_storage_raw_init(PutBytesStorage *storage, PutBytesObjectType object_type,
                         uint32_t total_size, PutBytesStorageInfo *info, uint32_t append_offset) {
  const MemoryLayout *layout = prv_get_layout_for_type(object_type);
  storage->impl_data = (void*) layout;

  storage->current_offset = layout->start_offset;

  // This erase operation will take awhile, so disable the task watchdog for the current task
  // while we're doing this.
  bool previous_system_task_watchdog_state = task_watchdog_mask_get(PebbleTask_KernelBackground);
  if (previous_system_task_watchdog_state) {
    task_watchdog_mask_clear(PebbleTask_KernelBackground);
  }

  if (append_offset == 0) {
    // By erasing the entire region we make it more likely for 'pb_storage_raw_get_status' to
    // recover the correct location.
    flash_region_erase_optimal_range(layout->start_address, layout->start_address,
        layout->end_address, layout->end_address);
  } else {
    // Some data we want has already been written, just continue from last valid location!
    storage->current_offset += append_offset;
  }

  if (previous_system_task_watchdog_state) {
    task_watchdog_mask_set(PebbleTask_KernelBackground);
  }

  return true;
}

uint32_t pb_storage_raw_get_max_size(PutBytesObjectType object_type) {
  const MemoryLayout *layout = prv_get_layout_for_type(object_type);
  return layout->end_address - layout->start_address;
}

void pb_storage_raw_write(PutBytesStorage *storage, uint32_t offset, const uint8_t *buffer,
                          uint32_t length) {
  const MemoryLayout *layout = storage->impl_data;

  const uint32_t flash_address = layout->start_address + offset;
  flash_write_bytes(buffer, flash_address, length);
}

uint32_t pb_storage_raw_calculate_crc(PutBytesStorage *storage, PutBytesCrcType crc_type) {
  const MemoryLayout *layout = storage->impl_data;

  const unsigned int start_address = layout->start_address + layout->start_offset;
  const unsigned int length = storage->current_offset - layout->start_offset;
  if (crc_type == PutBytesCrcType_Legacy) {
    return flash_calculate_legacy_defective_checksum(start_address, length);
  }

  return flash_crc32(start_address, length);
}

void pb_storage_raw_deinit(PutBytesStorage *storage, bool is_success) {
  // Nothing to do
}
