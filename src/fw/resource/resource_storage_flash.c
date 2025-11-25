/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "resource_storage_flash.h"
#include "resource_storage_impl.h"

#include "drivers/flash.h"
#include "kernel/pbl_malloc.h"
#include "resource/resource_version.auto.h"
#include "services/normal/process_management/app_storage.h"
#include "system/bootbits.h"
#include "system/logging.h"
#include "util/size.h"

#include <stdlib.h>

static const SystemResourceBank s_resource_banks[] = {
  {
    .begin = FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN,
    .end = FLASH_REGION_SYSTEM_RESOURCES_BANK_0_END,
  },
  {
    .begin = FLASH_REGION_SYSTEM_RESOURCES_BANK_1_BEGIN,
    .end = FLASH_REGION_SYSTEM_RESOURCES_BANK_1_END,
  },
};

//! Index into s_resource_banks
static unsigned int s_active_bank = 0;
#define BANK s_resource_banks[s_active_bank]

//! Set to true if we've scanned the available resource banks and determined one of them had valid
//! resources in it.
static bool s_valid_resources_found = false;

const ResourceStoreImplementation g_system_bank_impl;

static void resource_storage_system_bank_init(void) {
  boot_bit_clear(BOOT_BIT_NEW_SYSTEM_RESOURCES_AVAILABLE);

  ResourceStoreEntry entry = {
    .id = 0, // resource id 0 means the store itself
    .impl = &g_system_bank_impl,
    .length = ENTRY_LENGTH_UNSET
  };

  // Increment s_active_bank and call resource_storage_generic_check for each value to find
  // a bank that's valid.
  for (s_active_bank = 0; s_active_bank < ARRAY_LENGTH(s_resource_banks); ++s_active_bank) {
    PBL_LOG(LOG_LEVEL_INFO, "Checking bank %u for system resources", s_active_bank);
    if (resource_storage_generic_check(SYSTEM_APP, 0, &entry, &SYSTEM_RESOURCE_VERSION)) {
      PBL_LOG(LOG_LEVEL_INFO, "Valid system resources found!");
      s_valid_resources_found = true;
      return;
    }
  }

  // Welp, we found nothing. Leave s_valid_resources_found as false and when resource_storage_check
  // is called as part of system_resource_init we'll complain and handle missing resources.
}

// TODO PBL-21009: Move this somewhere else.
const SystemResourceBank *resource_storage_flash_get_unused_bank(void) {
  size_t unused_bank_index;
  if (s_valid_resources_found) {
    unused_bank_index = (s_active_bank + 1) % ARRAY_LENGTH(s_resource_banks);
  } else {
    static int s_unused_bank_index = -1;

    if (s_unused_bank_index == -1) {
      // A crude form of wear levelling to try and keep BB2s in infra happy
      //
      // For real watches, the only time this should happen is during initial onboarding. (If we
      // are in normal FW, one of the resource banks _must_ be valid.) We only call this once
      // because we want to target the same bank when both are unused so features like resumable
      // resource updates work as expected. We reset the bank on boot to make our watches a little
      // more resilient to the scenario where one of the resource banks has gone completely bad
      s_unused_bank_index = rand() % ARRAY_LENGTH(s_resource_banks);
    }
    unused_bank_index = s_unused_bank_index;
  }
  return &s_resource_banks[unused_bank_index];
}

static uint32_t resource_storage_system_bank_metadata_size(ResourceStoreEntry *entry) {
  return SYSTEM_STORE_METADATA_BYTES;
}

// PBL-28517 investigation
extern uint8_t pbl_28517_flash_impl_get_status_register(uint32_t sector_addr);

static uint32_t resource_storage_system_bank_get_crc(ResourceStoreEntry *entry, uint32_t num_bytes,
                                                     uint32_t entry_offset) {
#if (PLATFORM_SNOWY || PLATFORM_SPALDING) && !RELEASE && !UNITTEST
  // PBL-28517 investigation
  if (entry_offset == 0) {
    // We're calculating the CRC of the whole bank. Before we do this, let's save the status
    // register for each sector so we can see if the flash is in a funny state.

    for (int i = 0; (i * SECTOR_SIZE_BYTES) < (int) num_bytes; ++i) {
      const uint32_t addr = (BANK.begin + (i * SECTOR_SIZE_BYTES)) & SECTOR_ADDR_MASK;

      uint8_t status_reg = pbl_28517_flash_impl_get_status_register(addr);
      uint32_t crc = flash_calculate_legacy_defective_checksum(addr, SECTOR_SIZE_BYTES);

      PBL_LOG(LOG_LEVEL_DEBUG, "PBL-28517 Sector 0x%"PRIx32" Status 0x%"PRIx8" CRC 0x%"PRIx32,
              addr, status_reg, crc);
    }
  }
#endif

  uint32_t start_offset = resource_store_get_metadata_size(entry) + entry_offset;
  return flash_calculate_legacy_defective_checksum(
      BANK.begin + start_offset, num_bytes);
}

static uint32_t resource_storage_system_bank_read(ResourceStoreEntry *entry, uint32_t offset,
                                                  void *data, size_t num_bytes) {
  flash_read_bytes(data, BANK.begin + offset, num_bytes);
  return num_bytes;
}

#if CAPABILITY_HAS_MAPPABLE_FLASH
bool resource_storage_flash_bytes_are_readonly(const void *bytes) {
  return (bytes > (void *)FLASH_MEMORY_MAPPABLE_ADDRESS) &&
         (bytes < (void *)(FLASH_MEMORY_MAPPABLE_ADDRESS + FLASH_MEMORY_MAPPABLE_SIZE));
}

static const uint8_t *resource_storage_system_bank_readonly_bytes(ResourceStoreEntry *entry,
                                                                  bool has_privileged_access) {
  if (!has_privileged_access) {
    return NULL;
  }
  return (uint8_t *)(uintptr_t)(FLASH_MEMORY_MAPPABLE_ADDRESS + BANK.begin + entry->offset);
}

#else

bool resource_storage_flash_bytes_are_readonly(const void *bytes) {
  return false;
}

static const uint8_t *resource_storage_system_bank_readonly_bytes(ResourceStoreEntry *entry,
                                                                  bool has_privileged_access) {
  return NULL;
}

#endif // CAPABILITY_HAS_MAPPABLE_FLASH

static void resource_storage_system_bank_clear(ResourceStoreEntry *entry) {
  uint8_t buffer[MANIFEST_SIZE] = {0};
  flash_write_bytes(buffer, BANK.begin, MANIFEST_SIZE);
}


bool resource_storage_system_bank_check(ResAppNum app_num, uint32_t resource_id,
                                        ResourceStoreEntry *entry,
                                        const ResourceVersion *expected_version) {
  if (!s_valid_resources_found) {
    // We determined that we had no valid banks during init(), return false.
    return false;
  }

  // Are we checking the store itself?
  if (resource_id == 0) {
    // We've already verified that the bank was good at init(), just return true.
    return true;
  }

  // We're checking a specific resource, delegate this to the generic method.
  return resource_storage_generic_check(app_num, resource_id, entry, expected_version);
}

static bool resource_storage_system_bank_find_resource(ResourceStoreEntry *entry,
                                                       ResAppNum app_num, uint32_t resource_id) {
  return app_num == SYSTEM_APP && s_valid_resources_found;
}

const ResourceStoreImplementation g_system_bank_impl = {
  .type = ResourceStoreTypeSystemBank,

  .init = resource_storage_system_bank_init,
  .clear = resource_storage_system_bank_clear,
  .check = resource_storage_system_bank_check,

  .metadata_size = resource_storage_system_bank_metadata_size,
  .find_resource = resource_storage_system_bank_find_resource,
  .get_resource = resource_storage_generic_get_resource,

  .get_length = resource_storage_generic_get_length,
  .get_crc = resource_storage_system_bank_get_crc,
  .write = resource_storage_generic_write,
  .read = resource_storage_system_bank_read,
  .readonly_bytes = resource_storage_system_bank_readonly_bytes,

  .watch = resource_storage_generic_watch,
  .unwatch = resource_storage_generic_unwatch,
};
