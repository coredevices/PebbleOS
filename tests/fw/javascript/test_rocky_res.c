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

#include "clar.h"

#include "test_jerry_port_common.h"

#include "applib/rockyjs/rocky.h"
#include "applib/rockyjs/rocky_res.h"

// Fakes
#include "fake_app_timer.h"
#include "fake_time.h"

// Stubs
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_sleep.h"
#include "stubs_serial.h"
#include "stubs_syscalls.h"
#include "stubs_sys_exit.h"


// instead of including internal jerry script headers here and pulling the whole dependency we will
// duplicate this value here instead
// if this fails, duplicate value from third_party/jerryscript/jerryscript/jerry-core/jerry-snapshot.h
#define JERRY_SNAPSHOT_VERSION (6u)

void app_event_loop_common(void) {}

bool sys_get_current_app_is_rocky_app(void) {
  return true;
}

size_t heap_bytes_free(void) {
  return 123456;
}

static uint32_t s_resource_storage_get_num_entries__result;
uint32_t resource_storage_get_num_entries(ResAppNum app_num, uint32_t resource_id) {
  return s_resource_storage_get_num_entries__result;
}

void rocky_api_watchface_init(void){}
void rocky_api_deinit(void){}

size_t resource_size(ResAppNum app_num, uint32_t id) {
  return 0;
}

bool resource_is_valid(ResAppNum app_num, uint32_t resource_id) {
  return true;
}

int process_metadata_get_res_bank_num(const PebbleProcessMd *md) {
  return 123;
}

size_t resource_load_byte_range_system(ResAppNum app_num, uint32_t id, uint32_t start_offset, uint8_t *data, size_t num_bytes) {
  const size_t some_bytes = 20; // a real snapshot is larger than just the header

  // in our test setup, we will treat resource
  // 10 as an invalid snapshot header, and
  // 20 as a valid one
  switch (id) {
    case 10: {
      cl_assert(num_bytes >= sizeof(RockySnapshotHeader));
      *(RockySnapshotHeader*)data = (RockySnapshotHeader) {
        .version = 123, // invalid
      };
      return sizeof(RockySnapshotHeader);
    }
    case 20: {
      const size_t result = sizeof(RockySnapshotHeader) + sizeof(uint64_t);
      cl_assert(num_bytes >= result);

      RockySnapshotHeader *header = (RockySnapshotHeader*)data;
      *header = ROCKY_EXPECTED_SNAPSHOT_HEADER;
      // first uint64_t after our header is the jerry script buffer which starts with a version
      *(uint64_t*)(header + 1) = JERRY_SNAPSHOT_VERSION;
      return result;
    }
    default:
      return 0;
  }
}

void test_rocky_res__initialize(void) {
  s_resource_storage_get_num_entries__result = 0;
}

void test_rocky_res__no_snapshot(void) {
  s_resource_storage_get_num_entries__result = 5;
  cl_assert_equal_b(false, rocky_app_has_compatible_bytecode_res(123));
}

void test_rocky_res__only_invalid_snapshot(void) {
  s_resource_storage_get_num_entries__result = 15;
  cl_assert_equal_b(false, rocky_app_has_compatible_bytecode_res(123));
}

void test_rocky_res__valid_snapshot(void) {
  s_resource_storage_get_num_entries__result = 25;
  cl_assert_equal_b(true, rocky_app_has_compatible_bytecode_res(123));
}
