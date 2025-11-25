/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_put_bytes_storage_mem.h"

#include "clar_asserts.h"

#include "kernel/pbl_malloc.h"
#include "services/common/put_bytes/put_bytes_storage_internal.h"
#include "system/passert.h"

#define FAKE_STORAGE_MAX_SIZE (512 * 1024)

typedef struct FakePutBytesStorageData {
  PutBytesStorageInfo *info;
  bool last_is_success;
  uint32_t crc;
  uint32_t total_size;
  uint8_t buffer[FAKE_STORAGE_MAX_SIZE];
} FakePutBytesStorageData;

FakePutBytesStorageData s_storage_data;

bool pb_storage_raw_get_status(PutBytesObjectType obj_type,  PbInstallStatus *status) {
  return false;
}

static bool fake_pb_storage_mem_init(PutBytesStorage *storage, PutBytesObjectType object_type,
                                     uint32_t total_size, PutBytesStorageInfo *info,
                                     uint32_t append_offset) {
  // This fake only supports one put bytes storage to be init'd at a time.
  PBL_ASSERTN(!s_storage_data.total_size);
  size_t buffer_size = total_size + sizeof(FirmwareDescription);
  memset(s_storage_data.buffer, 0, sizeof(s_storage_data.buffer));
  s_storage_data.total_size = buffer_size;
  PutBytesStorageInfo *info_copy = NULL;
  if (info) {
    info_copy = (PutBytesStorageInfo *)kernel_malloc_check(sizeof(PutBytesStorageInfo));
    *info_copy = *info;
  }
  s_storage_data.info = info_copy;
  storage->impl_data = &s_storage_data;

  // put_bytes_storage_raw.c is weird, it reserves space at the beginning for FirmwareDescription:
  storage->current_offset = sizeof(FirmwareDescription);
  return true;
}

uint32_t fake_pb_storage_mem_get_max_size(PutBytesObjectType object_type) {
  return FAKE_STORAGE_MAX_SIZE;
}

static void(*s_do_before_write)(void) = NULL;
static void fake_pb_storage_mem_write(PutBytesStorage *storage, uint32_t offset,
                                      const uint8_t *buffer, uint32_t length) {
  PBL_ASSERTN(s_storage_data.total_size);
  PBL_ASSERTN(offset + length <= s_storage_data.total_size);

  if (s_do_before_write) {
    s_do_before_write();
    s_do_before_write = NULL;
  }

  memcpy(s_storage_data.buffer + offset, buffer, length);
}

static uint32_t fake_pb_storage_mem_calculate_crc(PutBytesStorage *storage, PutBytesCrcType crc_type) {
  PBL_ASSERTN(storage->impl_data == &s_storage_data);
  return s_storage_data.crc;
}

static void prv_cleanup(void) {
  kernel_free(s_storage_data.info);
  s_storage_data.info = NULL;
  s_storage_data.total_size = 0;
}

static void fake_pb_storage_mem_deinit(PutBytesStorage *storage, bool is_success) {
  PBL_ASSERTN(storage->impl_data == &s_storage_data);
  prv_cleanup();
  s_storage_data.last_is_success = is_success;
}

const PutBytesStorageImplementation s_raw_implementation = {
  .init = fake_pb_storage_mem_init,
  .get_max_size = fake_pb_storage_mem_get_max_size,
  .write = fake_pb_storage_mem_write,
  .calculate_crc = fake_pb_storage_mem_calculate_crc,
  .deinit = fake_pb_storage_mem_deinit,
};

const PutBytesStorageImplementation s_file_implementation = {
  .init = fake_pb_storage_mem_init,
  .get_max_size = fake_pb_storage_mem_get_max_size,
  .write = fake_pb_storage_mem_write,
  .calculate_crc = fake_pb_storage_mem_calculate_crc,
  .deinit = fake_pb_storage_mem_deinit,
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fake manipulation

void fake_pb_storage_register_cb_before_write(void (*cb_before_write)(void)) {
  s_do_before_write = cb_before_write;
}

void fake_pb_storage_mem_reset(void) {
  prv_cleanup();
  s_storage_data = (FakePutBytesStorageData) {};
}

void fake_pb_storage_mem_set_crc(uint32_t crc) {
  s_storage_data.crc = crc;
}

bool fake_pb_storage_mem_get_last_success(void) {
  return s_storage_data.last_is_success;
}

void fake_pb_storage_mem_assert_contents_written(const uint8_t contents[], size_t size) {
  cl_assert_equal_m(contents, s_storage_data.buffer + sizeof(FirmwareDescription), size);
}

void fake_pb_storage_mem_assert_fw_description_written(const FirmwareDescription *fw_descr) {
  cl_assert_equal_m(fw_descr, s_storage_data.buffer, sizeof(*fw_descr));
}

#undef FAKE_STORAGE_MAX_SIZE
