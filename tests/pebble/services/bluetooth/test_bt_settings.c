/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/settings/settings.h>

#include <clar.h>
#include <fake_pbl_malloc.h>
#include <fake_pfs.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>

void test_bt_settings__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
}

void test_bt_settings__defaults(void) {
  cl_assert_equal_b(bt_settings_get_airplane_mode(), false);
}

void test_bt_settings__persist(void) {
  bt_settings_set_airplane_mode(true);
  cl_assert_equal_b(bt_settings_get_airplane_mode(), true);

  bt_settings_set_airplane_mode(false);
  cl_assert_equal_b(bt_settings_get_airplane_mode(), false);
}

void test_bt_settings__migrate(void) {
  const bool airplane_mode = true;
  struct pbl_settings_file file;
  struct pbl_fs_dirent entry;

  cl_assert_equal_i(pbl_settings_open(&file, "/gap_bonding_db", 4096), 0);
  cl_assert_equal_i(pbl_settings_set(&file, "AIRPLANE_MODE", sizeof("AIRPLANE_MODE"),
                                     &airplane_mode, sizeof(airplane_mode)),
                    0);
  pbl_settings_close(&file);

  cl_assert_equal_b(bt_settings_get_airplane_mode(), true);

  cl_assert(pbl_fs_stat("/gap_bonding_db", &entry) != 0);
}
