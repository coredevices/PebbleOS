/* SPDX-CopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//
// RAM-backed coredump storage for the Memfault SDK.
//
// On SF32LB52, HAL_PMU_Reboot() power-cycles the entire SoC so .noinit RAM
// does not survive across reboots. Instead of writing the Memfault coredump
// during the crash, we reconstruct the Memfault coredump from the 
// PebbleOS flash-based coredump after reboot. The RAM buffer only needs 
// to survive from boot-time reconstruction until the chunk collector packetizes the data.
//
// The storage buffer is dynamically allocated when a coredump is reconstructed
// and freed when the SDK clears the storage after all chunks have been read.
//

#include <string.h>

#include "memfault/components.h"
#include "memfault/panics/platform/coredump.h"
#include "memfault_pebble_coredump.h"

#include "kernel/pbl_malloc.h"

#define COREDUMP_STORAGE_SIZE MEMFAULT_PLATFORM_COREDUMP_STORAGE_RAM_SIZE
#define COREDUMP_SECTOR_SIZE 4096

// Dynamically allocated coredump storage buffer.
static uint8_t *s_coredump_storage;
static size_t s_coredump_storage_size;

void memfault_coredump_storage_alloc(void) {
  if (s_coredump_storage != NULL) {
    return;  // Already allocated
  }
  s_coredump_storage = kernel_zalloc(COREDUMP_STORAGE_SIZE);
  if (s_coredump_storage != NULL) {
    s_coredump_storage_size = COREDUMP_STORAGE_SIZE;
  }
}

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  *info = (sMfltCoredumpStorageInfo){
    .size = s_coredump_storage_size,
    .sector_size = COREDUMP_SECTOR_SIZE,
  };
}

bool memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                             size_t read_len) {
  if (s_coredump_storage == NULL ||
      (offset + read_len) > s_coredump_storage_size) {
    return false;
  }
  memcpy(data, &s_coredump_storage[offset], read_len);
  return true;
}

bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                              size_t data_len) {
  if (s_coredump_storage == NULL ||
      (offset + data_len) > s_coredump_storage_size) {
    return false;
  }
  memcpy(&s_coredump_storage[offset], data, data_len);
  return true;
}

bool memfault_platform_coredump_storage_erase(uint32_t offset,
                                              size_t erase_size) {
  if (s_coredump_storage == NULL ||
      (offset + erase_size) > s_coredump_storage_size) {
    return false;
  }
  memset(&s_coredump_storage[offset], 0, erase_size);
  return true;
}

void memfault_platform_coredump_storage_clear(void) {
  // The SDK calls this after the packetizer has finished reading all chunks.
  // Mark the PebbleOS flash coredump as exported so it won't be reconstructed
  // again on subsequent reboots.
  memfault_pebble_coredump_mark_exported();

  // Free the dynamically allocated storage buffer.
  kernel_free(s_coredump_storage);
  s_coredump_storage = NULL;
  s_coredump_storage_size = 0;
}

size_t memfault_platform_sanitize_address_range(void *start_addr,
                                                size_t desired_size) {
  // SF32LB52 SRAM: main + worker + app RAM at 0x20000000
  const uint32_t ram_start = 0x20000000;
  const uint32_t ram_end = 0x20078000;

  const uint32_t addr = (uint32_t)(uintptr_t)start_addr;
  if (addr >= ram_start && addr < ram_end) {
    if (desired_size > (ram_end - addr)) {
      return ram_end - addr;
    }
    return desired_size;
  }

  // Invalid address range
  return 0;
}

// We reconstruct the coredump from the PebbleOS flash coredump after reboot
// rather than collecting regions at crash time, so this returns an empty list.
// The Memfault SDK's memfault_coredump_save() is called directly with the
// reconstructed data.
const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(
    const sCoredumpCrashInfo *crash_info, size_t *num_regions) {
  *num_regions = 0;
  return NULL;
}
