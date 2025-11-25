/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/bluetooth/bluetooth_persistent_storage.h"
#include "services/common/bluetooth/local_addr.h"

#include <bluetooth/bluetooth_types.h>

#include <stdbool.h>

#include <clar.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fakes / Stubs

#include "stubs_bt_lock.h"
#include "stubs_logging.h"
#include "stubs_passert.h"

static bool s_last_driver_allow_cycling;
BTDeviceAddress s_last_driver_addr;
bool s_last_driver_addr_is_null;
void bt_driver_set_local_address(bool allow_cycling,
                                 const BTDeviceAddress *pinned_address) {
  s_last_driver_allow_cycling = allow_cycling;
  if (pinned_address) {
    s_last_driver_addr_is_null = false;
    s_last_driver_addr = *pinned_address;
  } else {
    s_last_driver_addr_is_null = true;
    memset(&s_last_driver_addr, 0x00, sizeof(s_last_driver_addr));
  }
  return cl_mock_type(void);
}

BTDeviceAddress s_last_bt_persist_pinned_addr;
bool s_last_bt_persist_pinned_addr_is_null;

bool bt_persistent_storage_get_ble_pinned_address(BTDeviceAddress *address_out) {
  if (s_last_bt_persist_pinned_addr_is_null) {
    return false;
  }
  if (address_out) {
    *address_out = s_last_bt_persist_pinned_addr;
  }
  return true;
}

bool bt_persistent_storage_set_ble_pinned_address(const BTDeviceAddress *addr) {
  if (addr) {
    s_last_bt_persist_pinned_addr_is_null = false;
    s_last_bt_persist_pinned_addr = *addr;
  } else {
    s_last_bt_persist_pinned_addr_is_null = true;
    memset(&s_last_bt_persist_pinned_addr, 0x00, sizeof(s_last_bt_persist_pinned_addr));
  }
  return cl_mock_type(bool);
}

bool bt_persistent_storage_has_pinned_ble_pairings(void) {
  return cl_mock_type(bool);
}

#define TEST_PINNED_ADDR_1 ((BTDeviceAddress){ .octets = { 0x11, 0x22, 0x33, 0x33, 0x44, 0x55 } })
#define TEST_PINNED_ADDR_2 ((BTDeviceAddress){ .octets = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff } })

bool bt_driver_id_generate_private_resolvable_address(BTDeviceAddress *root_pinned_address_out) {
  *root_pinned_address_out = TEST_PINNED_ADDR_1;
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests

#define TEST_BONDING_ID ((BTBondingID) 1)

static void prv_init_no_pinnings_no_pinned_address(void) {
  cl_will_return(bt_persistent_storage_has_pinned_ble_pairings, false);
  cl_will_return(bt_persistent_storage_set_ble_pinned_address, true);
  cl_will_return(bt_driver_set_local_address, 0);
  s_last_bt_persist_pinned_addr_is_null = true;
  bt_local_addr_init();
}

static void prv_assert_driver_addr_is_null(void) {
  cl_assert_equal_b(s_last_driver_addr_is_null, true);
  cl_assert_equal_m(&s_last_driver_addr, &(BTDeviceAddress){}, sizeof(s_last_driver_addr));
}

static void prv_assert_driver_addr_is_addr1(void) {
  cl_assert_equal_b(s_last_driver_addr_is_null, false);
  cl_assert_equal_m(&s_last_driver_addr, &TEST_PINNED_ADDR_1, sizeof(s_last_driver_addr));
}

static void prv_assert_driver_addr_is_addr2(void) {
  cl_assert_equal_b(s_last_driver_addr_is_null, false);
  cl_assert_equal_m(&s_last_driver_addr, &TEST_PINNED_ADDR_2, sizeof(s_last_driver_addr));
}

void test_local_addr__initialize(void) {
  memset(&s_last_bt_persist_pinned_addr, 0xff, sizeof(s_last_bt_persist_pinned_addr));
  memset(&s_last_driver_addr, 0xff, sizeof(s_last_driver_addr));
  s_last_bt_persist_pinned_addr_is_null = false;
  s_last_driver_allow_cycling = false;
  s_last_driver_addr_is_null = false;
}

void test_local_addr__cleanup(void) {
}

void test_local_addr__init_generates_pinned_address_if_needed(void) {
  prv_init_no_pinnings_no_pinned_address();
  cl_assert_equal_b(false, s_last_bt_persist_pinned_addr_is_null);
  cl_assert_equal_m(&s_last_bt_persist_pinned_addr,
                    &TEST_PINNED_ADDR_1, sizeof(s_last_bt_persist_pinned_addr));
  cl_assert_equal_b(true, s_last_driver_allow_cycling);
}

void test_local_addr__init_loads_stored_pinned_address(void) {
  cl_will_return(bt_persistent_storage_has_pinned_ble_pairings, true);
  cl_will_return(bt_persistent_storage_set_ble_pinned_address, true);
  cl_will_return(bt_driver_set_local_address, 0);
  s_last_bt_persist_pinned_addr_is_null = false;
  s_last_bt_persist_pinned_addr = TEST_PINNED_ADDR_2;
  bt_local_addr_init();
  cl_assert_equal_b(false, s_last_driver_allow_cycling);
  prv_assert_driver_addr_is_addr2();
}

void test_local_addr__pause_resume(void) {
  prv_init_no_pinnings_no_pinned_address();

  // flip to make sure that bt_local_addr_pause_cycling() will set these again:
  s_last_driver_allow_cycling = true;
  s_last_driver_addr_is_null = false;

  cl_will_return(bt_driver_set_local_address, 0);
  bt_local_addr_pause_cycling();
  cl_assert_equal_b(false, s_last_driver_allow_cycling);
  // Check that it's using the pinned address that was generated upon initialization:
  prv_assert_driver_addr_is_addr1();

  bt_local_addr_pause_cycling();
  // Already paused, shouldn't result in a bt_driver_set_local_address() call.

  bt_local_addr_resume_cycling();
  // Still paused, shouldn't result in a bt_driver_set_local_address() call.

  // flip to make sure that bt_local_addr_pause_cycling() will set these again:
  s_last_driver_allow_cycling = true;
  s_last_driver_addr_is_null = false;

  cl_will_return(bt_driver_set_local_address, 0);
  bt_local_addr_resume_cycling();
  cl_assert_equal_b(true, s_last_driver_allow_cycling);
  prv_assert_driver_addr_is_null();
}

void test_local_addr__pin_unpin(void) {
  prv_init_no_pinnings_no_pinned_address();

  // Pin:
  cl_will_return(bt_driver_set_local_address, 0);
  cl_will_return(bt_persistent_storage_has_pinned_ble_pairings, true);
  bt_local_addr_pin(&TEST_PINNED_ADDR_1);
  bt_local_addr_handle_bonding_change(TEST_BONDING_ID, BtPersistBondingOpDidAdd);
  cl_assert_equal_b(false, s_last_driver_allow_cycling);
  prv_assert_driver_addr_is_addr1();

  // Unpin (done implicitly when bonding is removed):
  cl_will_return(bt_persistent_storage_has_pinned_ble_pairings, false);
  cl_will_return(bt_driver_set_local_address, 0);
  bt_local_addr_handle_bonding_change(TEST_BONDING_ID, BtPersistBondingOpWillDelete);
  cl_assert_equal_b(true, s_last_driver_allow_cycling);
  prv_assert_driver_addr_is_null();
}

void test_local_addr__pause_then_pin(void) {
  prv_init_no_pinnings_no_pinned_address();

  // Pause:
  cl_will_return(bt_driver_set_local_address, 0);
  bt_local_addr_pause_cycling();
  cl_assert_equal_b(false, s_last_driver_allow_cycling);
  prv_assert_driver_addr_is_addr1();

  // Pin, expect pinned address to be sent to BT driver:
  cl_will_return(bt_driver_set_local_address, 0);
  cl_will_return(bt_persistent_storage_has_pinned_ble_pairings, true);
  bt_local_addr_pin(&TEST_PINNED_ADDR_1);
  bt_local_addr_handle_bonding_change(TEST_BONDING_ID, BtPersistBondingOpDidAdd);
  cl_assert_equal_b(false, s_last_driver_allow_cycling);
  prv_assert_driver_addr_is_addr1();
}
