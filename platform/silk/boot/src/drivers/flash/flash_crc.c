/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/flash.h"

#include "util/crc32.h"

#define CRC_CHUNK_SIZE 1024

uint32_t flash_calculate_checksum(uint32_t flash_addr, uint32_t num_bytes) {
  uint8_t buffer[CRC_CHUNK_SIZE];

  uint32_t crc = CRC32_INIT;

  while (num_bytes > CRC_CHUNK_SIZE) {
    flash_read_bytes(buffer, flash_addr, CRC_CHUNK_SIZE);
    crc = crc32(crc, buffer, CRC_CHUNK_SIZE);

    num_bytes -= CRC_CHUNK_SIZE;
    flash_addr += CRC_CHUNK_SIZE;
  }

  flash_read_bytes(buffer, flash_addr, num_bytes);
  return crc32(crc, buffer, num_bytes);
}
