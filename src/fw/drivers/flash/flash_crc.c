/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>

#include <stdint.h>

#include <pbl/logging/logging.h>
#include "kernel/pbl_malloc.h"
#include "pbl/util/crc32.h"
#include "util/legacy_checksum.h"

PBL_LOG_MODULE_DECLARE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

static size_t prv_allocate_buffer(void **buffer) {
  size_t chunk_size = 1024;

  *buffer = kernel_malloc(chunk_size);
  if (*buffer == NULL) {
    PBL_LOG_WRN("Insufficient memory for a large CRC buffer, going slow");
    chunk_size = 128;
    *buffer = kernel_malloc_check(chunk_size);
  }

  return chunk_size;
}

uint32_t pbl_flash_crc32(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  void *buffer;
  size_t chunk_size = prv_allocate_buffer(&buffer);
  uint32_t crc = CRC32_INIT;

  while (len > 0) {
    size_t chunk = (len < chunk_size) ? len : chunk_size;
    pbl_flash_read(dev, addr, buffer, chunk);
    crc = crc32(crc, buffer, chunk);
    addr += chunk;
    len -= chunk;
  }

  kernel_free(buffer);

  return crc;
}

uint32_t pbl_flash_legacy_checksum(const struct pbl_flash_device *dev, uint32_t addr, size_t len) {
  void *buffer;
  size_t chunk_size = prv_allocate_buffer(&buffer);
  LegacyChecksum checksum;

  legacy_defective_checksum_init(&checksum);
  while (len > 0) {
    size_t chunk = (len < chunk_size) ? len : chunk_size;
    pbl_flash_read(dev, addr, buffer, chunk);
    legacy_defective_checksum_update(&checksum, buffer, chunk);
    addr += chunk;
    len -= chunk;
  }

  kernel_free(buffer);

  return legacy_defective_checksum_finish(&checksum);
}
