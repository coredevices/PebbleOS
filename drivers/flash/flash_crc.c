/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>

#include <kernel/pbl_malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/crc/crc.h>

#include <stdint.h>

PBL_LOG_MODULE_DECLARE(driver_flash, CONFIG_DRIVER_FLASH_LOG_LEVEL);

static size_t prv_allocate_crc_buffer(void **buffer) {
  // Try to allocate a big buffer for reading flash data. If we can't,
  // use a smaller one.
  unsigned int chunk_size = 1024;
  *buffer = kernel_malloc(chunk_size);
  if (!*buffer) {
    PBL_LOG_WRN("Insufficient memory for a large CRC buffer, going slow");

    chunk_size = 128;
    *buffer = kernel_malloc_check(chunk_size);
  }
  return chunk_size;
}

uint32_t flash_crc32(uint32_t flash_addr, uint32_t num_bytes) {
  void *buffer;
  unsigned int chunk_size = prv_allocate_crc_buffer(&buffer);

  uint32_t crc = 0;
  while (num_bytes > chunk_size) {
    flash_read_bytes(buffer, flash_addr, chunk_size);
    crc = pbl_crc32(crc, buffer, chunk_size);

    num_bytes -= chunk_size;
    flash_addr += chunk_size;
  }

  flash_read_bytes(buffer, flash_addr, num_bytes);
  crc = pbl_crc32(crc, buffer, num_bytes);

  kernel_free(buffer);

  return crc;
}

uint32_t flash_crc32_legacy(uint32_t flash_addr, uint32_t num_bytes) {
  void *buffer;
  unsigned int chunk_size = prv_allocate_crc_buffer(&buffer);

  struct pbl_crc32_legacy checksum;
  pbl_crc32_legacy_init(&checksum);

  while (num_bytes > chunk_size) {
    flash_read_bytes(buffer, flash_addr, chunk_size);
    pbl_crc32_legacy_update(&checksum, buffer, chunk_size);

    num_bytes -= chunk_size;
    flash_addr += chunk_size;
  }

  flash_read_bytes(buffer, flash_addr, num_bytes);
  pbl_crc32_legacy_update(&checksum, buffer, num_bytes);

  kernel_free(buffer);

  return pbl_crc32_legacy_finish(&checksum);
}
