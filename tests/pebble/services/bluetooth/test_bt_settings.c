/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>
#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/settings/settings.h>

#include <clar.h>
#include <fake_events.h>
#include <fake_pbl_malloc.h>
#include <fake_pfs.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>

void test_bt_settings__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  fake_event_init();
  bt_settings_init();
}

void test_bt_settings__defaults(void) {
  struct pbl_pp_capabilities capabilities = {.flags = 1};

  cl_assert_equal_b(bt_settings_get_airplane_mode(), false);
  cl_assert_equal_b(bt_settings_is_unfaithful(), true);
  bt_settings_get_capabilities(&capabilities);
  cl_assert_equal_i(capabilities.flags, 0);
}

void test_bt_settings__persist(void) {
  const struct pbl_pp_capabilities capabilities = {.flags = 0x5};
  struct pbl_pp_capabilities capabilities_out;

  bt_settings_set_airplane_mode(true);
  bt_settings_set_unfaithful(false);
  bt_settings_set_capabilities(&capabilities);
  cl_assert_equal_i(fake_event_get_count(), 1);

  bt_settings_init();

  cl_assert_equal_b(bt_settings_get_airplane_mode(), true);
  cl_assert_equal_b(bt_settings_is_unfaithful(), false);
  bt_settings_get_capabilities(&capabilities_out);
  cl_assert_equal_i(capabilities_out.flags, capabilities.flags);

  bt_settings_set_capabilities(&capabilities);
  cl_assert_equal_i(fake_event_get_count(), 1);

  bt_settings_set_capabilities(nullptr);
  bt_settings_get_capabilities(&capabilities_out);
  cl_assert_equal_i(capabilities_out.flags, 0);
  cl_assert_equal_i(fake_event_get_count(), 2);
}

void test_bt_settings__migrate(void) {
  const bool airplane_mode = true;
  const bool unfaithful = false;
  const struct pbl_pp_capabilities capabilities = {.flags = 0x3};
  struct pbl_pp_capabilities capabilities_out;
  struct pbl_settings_file file;
  struct pbl_fs_dirent entry;

  cl_assert_equal_i(pbl_settings_open(&file, "/gap_bonding_db", 4096), 0);
  cl_assert_equal_i(pbl_settings_set(&file, "AIRPLANE_MODE", sizeof("AIRPLANE_MODE"),
                                     &airplane_mode, sizeof(airplane_mode)),
                    0);
  cl_assert_equal_i(pbl_settings_set(&file, "IS_UNFAITHFUL", sizeof("IS_UNFAITHFUL"), &unfaithful,
                                     sizeof(unfaithful)),
                    0);
  cl_assert_equal_i(pbl_settings_set(&file, "SYSTEM_CAPABILITIES", sizeof("SYSTEM_CAPABILITIES"),
                                     &capabilities, sizeof(capabilities)),
                    0);
  pbl_settings_close(&file);

  bt_settings_init();

  cl_assert_equal_b(bt_settings_get_airplane_mode(), true);
  cl_assert_equal_b(bt_settings_is_unfaithful(), false);
  bt_settings_get_capabilities(&capabilities_out);
  cl_assert_equal_i(capabilities_out.flags, capabilities.flags);

  cl_assert(pbl_fs_stat("/gap_bonding_db", &entry) != 0);
}
