/* SPDX-FileCopyrightText: 2026 Shashvat Prabhu */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/battery/battery_charge_limit.h>

#include <clar.h>

// Stubs
////////////////////////////////////
#include <fake_work.h>

extern bool s_limit_active;

static uint8_t s_pref_pct;
static bool s_charge_enabled;
static int s_force_update_count;

uint8_t shell_prefs_get_charge_limit_pct(void) {
  return s_pref_pct;
}

void battery_set_charge_enable(bool charging_enabled) {
  s_charge_enabled = charging_enabled;
}

void battery_state_force_update(void) {
  s_force_update_count++;
}

static PreciseBatteryChargeState prv_state(uint8_t pct, bool plugged) {
  return (PreciseBatteryChargeState){
    .pct = pct,
    .is_plugged = plugged,
    .is_charging = plugged,
  };
}

// Tests
////////////////////////////////////

void test_battery_charge_limit__initialize(void) {
  s_limit_active = false;
  s_pref_pct = 80;
  s_charge_enabled = true;
  s_force_update_count = 0;
  fake_work_cleanup();
}

void test_battery_charge_limit__cleanup(void) {
  fake_work_cleanup();
}

void test_battery_charge_limit__disabled_pref_never_limits(void) {
  s_pref_pct = CHARGE_LIMIT_PCT_OFF;
  battery_charge_limit_evaluate(prv_state(95, true));
  cl_assert(!battery_charge_limit_is_active());
  cl_assert(s_charge_enabled);
}

void test_battery_charge_limit__stops_at_limit(void) {
  battery_charge_limit_evaluate(prv_state(79, true));
  cl_assert(!battery_charge_limit_is_active());
  cl_assert(s_charge_enabled);

  battery_charge_limit_evaluate(prv_state(80, true));
  cl_assert(battery_charge_limit_is_active());
  cl_assert(!s_charge_enabled);
}

void test_battery_charge_limit__resumes_below_limit(void) {
  battery_charge_limit_evaluate(prv_state(80, true));
  cl_assert(!s_charge_enabled);

  battery_charge_limit_evaluate(prv_state(79, true));
  cl_assert(!battery_charge_limit_is_active());
  cl_assert(s_charge_enabled);
}

void test_battery_charge_limit__unplug_reenables_charging(void) {
  battery_charge_limit_evaluate(prv_state(85, true));
  cl_assert(!s_charge_enabled);

  battery_charge_limit_evaluate(prv_state(85, false));
  cl_assert(!battery_charge_limit_is_active());
  cl_assert(s_charge_enabled);

  // Plugging back in below the limit must charge normally.
  battery_charge_limit_evaluate(prv_state(50, true));
  cl_assert(!battery_charge_limit_is_active());
  cl_assert(s_charge_enabled);
}

void test_battery_charge_limit__disabling_pref_reenables_charging(void) {
  battery_charge_limit_evaluate(prv_state(90, true));
  cl_assert(!s_charge_enabled);

  s_pref_pct = CHARGE_LIMIT_PCT_OFF;
  battery_charge_limit_evaluate(prv_state(90, true));
  cl_assert(!battery_charge_limit_is_active());
  cl_assert(s_charge_enabled);
}

void test_battery_charge_limit__uses_configured_limit(void) {
  s_pref_pct = 60;
  battery_charge_limit_evaluate(prv_state(59, true));
  cl_assert(s_charge_enabled);

  battery_charge_limit_evaluate(prv_state(60, true));
  cl_assert(!s_charge_enabled);

  // Raising the limit above the current level resumes charging.
  s_pref_pct = 90;
  battery_charge_limit_evaluate(prv_state(60, true));
  cl_assert(!battery_charge_limit_is_active());
  cl_assert(s_charge_enabled);
}

void test_battery_charge_limit__recheck_only_while_active(void) {
  battery_charge_limit_evaluate(prv_state(50, true));
  cl_assert_equal_p(fake_work_get_next(), NULL);

  battery_charge_limit_evaluate(prv_state(80, true));
  struct pbl_delayed_work *work = fake_work_get_next();
  cl_assert(work != NULL);
  cl_assert_equal_i(fake_work_timeout_ms(work), 60 * 1000);

  fake_work_fire(work);
  cl_assert_equal_i(s_force_update_count, 1);

  battery_charge_limit_evaluate(prv_state(80, false));
  cl_assert(!fake_work_is_scheduled(work));
}
