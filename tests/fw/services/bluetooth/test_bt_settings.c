/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/bluetooth/bt_settings.h>
#include <pbl/services/filesystem/pfs.h>
#include <pbl/services/settings/settings_file.h>

#include <clar.h>
#include <fake_events.h>
#include <fake_pbl_malloc.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>

void test_bt_settings__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  fake_event_init();
  bt_settings_init();
}

void test_bt_settings__defaults(void) {
  PebbleProtocolCapabilities capabilities = {.flags = 1};

  cl_assert_equal_b(bt_settings_get_airplane_mode(), false);
  cl_assert_equal_b(bt_settings_is_unfaithful(), true);
  bt_settings_get_capabilities(&capabilities);
  cl_assert_equal_i(capabilities.flags, 0);
}

void test_bt_settings__persist(void) {
  const PebbleProtocolCapabilities capabilities = {.flags = 0x5};
  PebbleProtocolCapabilities capabilities_out;

  bt_settings_set_airplane_mode(true);
  bt_settings_set_unfaithful(false);
  bt_settings_set_capabilities(&capabilities);
  cl_assert_equal_i(fake_event_get_count(), 1);

  bt_settings_init();

  cl_assert_equal_b(bt_settings_get_airplane_mode(), true);
  cl_assert_equal_b(bt_settings_is_unfaithful(), false);
  bt_settings_get_capabilities(&capabilities_out);
  cl_assert_equal_i(capabilities_out.flags, capabilities.flags);

  // Unchanged: no event.
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
  const PebbleProtocolCapabilities capabilities = {.flags = 0x3};
  PebbleProtocolCapabilities capabilities_out;
  SettingsFile fd;

  cl_assert_equal_i(settings_file_open(&fd, "gap_bonding_db", 4096), S_SUCCESS);
  cl_assert_equal_i(settings_file_set(&fd, "AIRPLANE_MODE", sizeof("AIRPLANE_MODE"), &airplane_mode,
                                      sizeof(airplane_mode)),
                    S_SUCCESS);
  cl_assert_equal_i(settings_file_set(&fd, "IS_UNFAITHFUL", sizeof("IS_UNFAITHFUL"), &unfaithful,
                                      sizeof(unfaithful)),
                    S_SUCCESS);
  cl_assert_equal_i(settings_file_set(&fd, "SYSTEM_CAPABILITIES", sizeof("SYSTEM_CAPABILITIES"),
                                      &capabilities, sizeof(capabilities)),
                    S_SUCCESS);
  settings_file_close(&fd);

  bt_settings_init();

  cl_assert_equal_b(bt_settings_get_airplane_mode(), true);
  cl_assert_equal_b(bt_settings_is_unfaithful(), false);
  bt_settings_get_capabilities(&capabilities_out);
  cl_assert_equal_i(capabilities_out.flags, capabilities.flags);

  // The bonding database is gone: the stack imports its bond from shared PRF storage.
  cl_assert(pfs_open("gap_bonding_db", OP_FLAG_READ, FILE_TYPE_STATIC, 0) < 0);
}

void test_bt_settings__peer_name(void) {
  const struct pbl_bt_device_internal addr = {
    .address = {.octets = {1, 2, 3, 4, 5, 6}},
  };
  const struct pbl_bt_device_internal other = {
    .address = {.octets = {6, 5, 4, 3, 2, 1}},
  };
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];

  cl_assert(!bt_settings_get_peer_name(&addr, name));

  bt_settings_set_peer_name(&addr, "iPhone");
  cl_assert(bt_settings_get_peer_name(&addr, name));
  cl_assert_equal_s(name, "iPhone");
  cl_assert(!bt_settings_get_peer_name(&other, name));

  bt_settings_delete_peer_name(&addr);
  cl_assert(!bt_settings_get_peer_name(&addr, name));
}
