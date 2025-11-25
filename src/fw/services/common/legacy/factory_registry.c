/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/legacy/factory_registry.h"
#include "services/common/legacy/registry_private.h"

#include "flash_region/flash_region.h"

#define FACTORY_REGISTRY_NUM_OF_RECORDS 10
static Record s_factory_records[FACTORY_REGISTRY_NUM_OF_RECORDS];

static RegistryCursor s_factory_flash_cursor = {
  .address = FLASH_CURSOR_UNINITIALIZED,
  .begin = FACTORY_REGISTRY_FLASH_BEGIN,
  .end = FACTORY_REGISTRY_FLASH_END,
};

static Registry s_factory_registry = {
  .is_different_from_flash = false,
  .records = s_factory_records,
  .num_records = FACTORY_REGISTRY_NUM_OF_RECORDS,
  .registry_size_bytes = FACTORY_REGISTRY_NUM_OF_RECORDS * sizeof(Record),
  .total_buffer_size_bytes = FACTORY_REGISTRY_NUM_OF_RECORDS * sizeof(Record) +
      REGISTRY_HEADER_SIZE_BYTES,
  .cursor = &s_factory_flash_cursor,
};

void factory_registry_init(void) {
  registry_private_init(&s_factory_registry);
  registry_private_read_from_flash(&s_factory_registry);
}

int factory_registry_add(const char* key, const uint8_t key_length, const uint8_t* uuid,
                         const uint8_t description, const uint8_t* value, uint8_t value_length) {
  return registry_private_add(key, key_length, uuid, description, value, value_length,
                              &s_factory_registry);
}

Record* factory_registry_get(const char* key, const uint8_t key_length, const uint8_t* uuid) {
  return registry_private_get(key, key_length, uuid, &s_factory_registry);
}

int factory_registry_remove(const char* key, const uint8_t key_length, const uint8_t* uuid) {
  return registry_private_remove(key, key_length, uuid, &s_factory_registry);
}

void factory_registry_write_to_flash(void) {
  registry_private_write_to_flash(&s_factory_registry);
}

void factory_registry_remove_all(const uint8_t* uuid) {
  registry_private_remove_all(uuid, &s_factory_registry);
}


