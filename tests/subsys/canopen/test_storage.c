/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_test.h"
#include "storage.h"

#include <string.h>

#include <pbl/canopen/canopen.h>

#include <clar.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>

#define SAVE 0x65766173U
#define LOAD 0x64616f6cU

// The file system takes kernel locks: mounted from the test thread, before the node starts.
static void prv_mount(struct pbl_canopen *co) {
  fake_pfs_mount();
}

static void prv_run(void (*scenario)(void)) {
  ct_run(&storage, nullptr, prv_mount, scenario);
}

void test_storage__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  ct_init();
}

void test_storage__cleanup(void) {
}

static void prv_save_restore(void) {
  uint8_t status = 7U;
  char label[9] = {0};

  // On command, not autonomously
  cl_assert_equal_i(ct_sdo_read(0x1010, 0), 3U);
  cl_assert_equal_i(ct_sdo_read(0x1010, 1), 1U);
  cl_assert_equal_i(ct_sdo_read(0x1011, 3), 1U);
  cl_assert_equal_i(ct_sdo_abort(0x23U, 0x1010, 1, 0x12345678U), 0x08000020U);

  ct_sdo_write(0x1017, 0, 500U, 2U);
  ct_sdo_write(STORAGE_GAIN, 0, 42U, 4U);
  ct_sdo_write(STORAGE_LABEL, 0, 0x7a79U, 2U);
  cl_assert_equal_i(pbl_canopen_od_write(&ct_co, STORAGE_STATUS, 0, &status, 1U), 0);
  ct_sdo_write(0x1010, 1, SAVE, 4U);
  cl_assert_equal_i(ct_sdo_read(0x1010, 1), 1U);

  // Loaded on reset node; read-only entries are not parameters.
  ct_nmt(0x81);
  cl_assert_equal_i(ct_sdo_read(0x1017, 0), 500U);
  cl_assert_equal_i(ct_sdo_read(STORAGE_GAIN, 0), 42U);
  cl_assert_equal_i(pbl_canopen_od_read(&ct_co, STORAGE_LABEL, 0, label, sizeof(label)), 2);
  cl_assert_equal_s(label, "yz");
  cl_assert_equal_i(ct_sdo_read(STORAGE_STATUS, 0), 1U);

  // Reset communication loads the communication parameters only.
  ct_sdo_write(0x1017, 0, 200U, 2U);
  ct_sdo_write(STORAGE_GAIN, 0, 43U, 4U);
  ct_nmt(0x82);
  cl_assert_equal_i(ct_sdo_read(0x1017, 0), 500U);
  cl_assert_equal_i(ct_sdo_read(STORAGE_GAIN, 0), 43U);

  // Saving the application parameters only
  ct_sdo_write(0x1017, 0, 300U, 2U);
  ct_sdo_write(0x1010, 3, SAVE, 4U);
  ct_nmt(0x81);
  cl_assert_equal_i(ct_sdo_read(0x1017, 0), 500U);
  cl_assert_equal_i(ct_sdo_read(STORAGE_GAIN, 0), 43U);

  // Restoring the communication defaults, on the next reset
  cl_assert_equal_i(ct_sdo_abort(0x23U, 0x1011, 2, SAVE), 0x08000020U);
  ct_sdo_write(0x1011, 2, LOAD, 4U);
  cl_assert_equal_i(ct_sdo_read(0x1017, 0), 500U);
  ct_nmt(0x81);
  cl_assert_equal_i(ct_sdo_read(0x1017, 0), 100U);
  cl_assert_equal_i(ct_sdo_read(STORAGE_GAIN, 0), 43U);

  // And every default
  ct_sdo_write(0x1011, 1, LOAD, 4U);
  ct_nmt(0x81);
  cl_assert_equal_i(ct_sdo_read(STORAGE_GAIN, 0), 10U);
}

void test_storage__save_restore(void) {
  prv_run(prv_save_restore);
}

static void prv_saved(void) {
  ct_sdo_write(STORAGE_GAIN, 0, 99U, 4U);
  ct_sdo_write(0x1010, 1, SAVE, 4U);
}

static void prv_loaded(void) {
  cl_assert_equal_i(ct_sdo_read(STORAGE_GAIN, 0), 99U);
}

void test_storage__loaded_on_start(void) {
  prv_run(prv_saved);
  ct_init();
  prv_run(prv_loaded);
}
