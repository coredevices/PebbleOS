/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

void crc_init(void) {
}

uint32_t crc_calculate_bytes(const void* data, size_t data_length) {
  return 0;
}

uint32_t crc_calculate_flash(uint32_t address, unsigned int num_bytes) {
  return 0;
}

uint8_t crc8_calculate_bytes(const uint8_t *data, unsigned int data_length) {
  return 0;
}

void crc_calculate_incremental_start(void) {
}

void crc_calculate_incremental_stop(void) {
}

uint32_t crc_calculate_file(int fd, uint32_t offset, uint32_t num_bytes) {
  return 0;
}
