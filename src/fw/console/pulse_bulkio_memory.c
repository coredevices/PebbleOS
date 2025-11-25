/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pulse_bulkio_domain_handler.h"

#include "system/status_codes.h"
#include "util/attributes.h"

#include <stdint.h>
#include <string.h>

typedef struct PACKED MemoryEraseOptions {
  uint32_t address;
  uint32_t length;
} MemoryEraseOptions;

static int memory_domain_read(uint8_t *buf, uint32_t address, uint32_t length,
                                    void *context) {
  memcpy(buf, (void *)address, length);
  return length;
}

static int memory_domain_write(uint8_t *buf, uint32_t address, uint32_t length,
                                    void *context) {
  memcpy(buf, (void *)address, length);
  return length;
}

static int memory_domain_stat(uint8_t *resp, size_t resp_max_len, void *context) {
  return E_INVALID_OPERATION;
}

static status_t memory_domain_erase(uint8_t *packet_data, size_t length, uint8_t cookie) {
  if (length != sizeof(MemoryEraseOptions)) {
    return E_INVALID_ARGUMENT;
  }

  MemoryEraseOptions *options = (MemoryEraseOptions*)packet_data;

  memset((void *)options->address, 0x0, length);
  return S_SUCCESS;
}

static status_t memory_domain_open(uint8_t *packet_data, size_t length, void **resp) {
  return S_SUCCESS;
}

static status_t memory_domain_close(void *context) {
  return S_SUCCESS;
}

PulseBulkIODomainHandler pulse_bulkio_domain_memory = {
  .id = PulseBulkIODomainType_Memory,
  .open_proc = memory_domain_open,
  .close_proc = memory_domain_close,
  .read_proc = memory_domain_read,
  .write_proc = memory_domain_write,
  .stat_proc = memory_domain_stat,
  .erase_proc = memory_domain_erase
};
