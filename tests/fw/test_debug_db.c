/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "debug/legacy/debug_db.h"

#include "clar.h"

#include "system/version.h"

#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

// Stubs
/////////////////////////////////////////////
void flash_read_bytes(uint8_t* buffer, uint32_t start_addr, uint32_t buffer_size) {}
void flash_write_bytes(const uint8_t* buffer, uint32_t start_addr, uint32_t buffer_size) {}
void flash_erase_subsector_blocking(uint32_t subsector_addr) {}
bool version_copy_running_fw_metadata(FirmwareMetadata *out_metadata) { return false; }

// Tests
/////////////////////////////////////////////

void test_debug_db__initialize(void) {
}
void test_debug_db__cleanup(void) {
}

void test_debug_db__unitialized(void) {
  int index;
  uint8_t id;

  debug_db_determine_current_index((uint8_t []) { 255, 255, 255, 255 }, &index, &id);
  cl_assert_equal_i(index, 0);
  cl_assert_equal_i(id, 0);

  debug_db_determine_current_index((uint8_t []) { 0, 255, 255, 255 }, &index, &id);
  cl_assert_equal_i(index, 1);
  cl_assert_equal_i(id, 1);

  debug_db_determine_current_index((uint8_t []) { 0, 1, 255, 255 }, &index, &id);
  cl_assert_equal_i(index, 2);
  cl_assert_equal_i(id, 2);

  debug_db_determine_current_index((uint8_t []) { 0, 1, 2, 255 }, &index, &id);
  cl_assert_equal_i(index, 3);
  cl_assert_equal_i(id, 3);
}

void test_debug_db__basic(void) {
  int index;
  uint8_t id;

  debug_db_determine_current_index((uint8_t []) { 0, 1, 2, 3 }, &index, &id);
  cl_assert_equal_i(index, 0);
  cl_assert_equal_i(id, 4);

  debug_db_determine_current_index((uint8_t []) { 4, 1, 2, 3 }, &index, &id);
  cl_assert_equal_i(index, 1);
  cl_assert_equal_i(id, 5);

  debug_db_determine_current_index((uint8_t []) { 4, 5, 2, 3 }, &index, &id);
  cl_assert_equal_i(index, 2);
  cl_assert_equal_i(id, 6);

  debug_db_determine_current_index((uint8_t []) { 4, 5, 6, 3 }, &index, &id);
  cl_assert_equal_i(index, 3);
  cl_assert_equal_i(id, 7);
}

void test_debug_db__weird(void) {
  int index;
  uint8_t id;

  debug_db_determine_current_index((uint8_t []) { 0, 9, 6, 7 }, &index, &id);
  cl_assert_equal_i(index, 1);
  cl_assert_equal_i(id, 1);
}

