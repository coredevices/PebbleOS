/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/pairability.h>
#include <pbl/services/bluetooth/advertising.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/pairability.h>

#include <clar.h>
#include <fake_rtc.h>
#include <fake_sys_work.h>
#include <fake_work.h>

static size_t s_num_bonds;
static bool s_running;
static bool s_pairable;
static bool s_discoverable;
static int s_num_discoverable_calls;

size_t pbl_bt_bond_count(void) {
  return s_num_bonds;
}

bool bt_ctl_is_bluetooth_running(void) {
  return s_running;
}

void pbl_bt_pairability_set_enabled(bool enabled) {
  s_pairable = enabled;
}

void bt_advertising_set_discoverable(bool discoverable) {
  s_discoverable = discoverable;
  s_num_discoverable_calls++;
}

/* Lets the time pass, running what comes due and what that asks of the work queue. */
static void prv_spend_seconds(uint32_t seconds) {
  uint64_t remaining = (uint64_t)seconds * PBL_TICK_HZ;

  while (true) {
    struct pbl_delayed_work *work = fake_work_get_next();

    if (work == nullptr || work->timer.timeout.expires > remaining) {
      if (work != nullptr) {
        work->timer.timeout.expires -= remaining;
      }
      fake_rtc_increment_ticks(remaining);
      break;
    }

    remaining -= work->timer.timeout.expires;
    fake_rtc_increment_ticks(work->timer.timeout.expires);
    fake_work_fire(work);
  }

  fake_sys_work_callbacks_invoke_pending();
}

static void prv_assert_pairable(bool pairable) {
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_b(s_pairable, pairable);
  cl_assert_equal_b(s_discoverable, pairable);
}

void test_pairability__initialize(void) {
  fake_rtc_init(0, 0);
  s_num_bonds = 1;
  s_running = true;
  s_pairable = false;
  s_discoverable = false;

  bt_pairability_init();
  prv_assert_pairable(false);
  s_num_discoverable_calls = 0;
}

/* The references are the module's, and outlive a test. */
void test_pairability__cleanup(void) {
  bt_pairability_use_for_period(0);
  prv_spend_seconds(1);
  s_num_bonds = 1;
  bt_pairability_update_due_to_bonding_change();
  fake_sys_work_callbacks_invoke_pending();
  fake_work_cleanup();
}

void test_pairability__references_are_counted(void) {
  bt_pairability_use();
  bt_pairability_use();
  prv_assert_pairable(true);

  bt_pairability_release();
  prv_assert_pairable(true);

  bt_pairability_release();
  prv_assert_pairable(false);

  /* Advertising is only told of changes. */
  cl_assert_equal_i(s_num_discoverable_calls, 2);
}

void test_pairability__pairable_while_there_is_no_bond(void) {
  s_num_bonds = 0;
  bt_pairability_update_due_to_bonding_change();
  bt_pairability_update_due_to_bonding_change();
  prv_assert_pairable(true);

  s_num_bonds = 1;
  bt_pairability_update_due_to_bonding_change();
  prv_assert_pairable(false);

  /* One reference however often it is evaluated. */
  bt_pairability_update_due_to_bonding_change();
  prv_assert_pairable(false);
}

void test_pairability__period_ends(void) {
  bt_pairability_use_for_period(30);
  prv_assert_pairable(true);

  prv_spend_seconds(29);
  prv_assert_pairable(true);

  prv_spend_seconds(2);
  prv_assert_pairable(false);
}

void test_pairability__period_is_extended_with_one_reference(void) {
  bt_pairability_use_for_period(30);
  prv_spend_seconds(20);
  bt_pairability_use_for_period(30);
  prv_spend_seconds(20);
  prv_assert_pairable(true);

  prv_spend_seconds(11);
  prv_assert_pairable(false);
}

void test_pairability__period_can_be_ended(void) {
  /* Nothing to end. */
  bt_pairability_use_for_period(0);
  prv_assert_pairable(false);

  bt_pairability_use_for_period(30);
  prv_assert_pairable(true);

  bt_pairability_use_for_period(0);
  prv_spend_seconds(1);
  prv_assert_pairable(false);
}

void test_pairability__applied_once_the_stack_runs(void) {
  s_running = false;
  bt_pairability_use();
  fake_sys_work_callbacks_invoke_pending();
  cl_assert_equal_b(s_pairable, false);

  s_running = true;
  bt_pairability_init();
  prv_assert_pairable(true);

  bt_pairability_release();
  prv_assert_pairable(false);
}
