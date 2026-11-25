/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/bond.h>
#include <pbl/bluetooth/pairability.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/advertising.h>
#include <pbl/services/bluetooth/bluetooth_ctl.h>
#include <pbl/services/bluetooth/pairability.h>

#include <kernel/pebble_tasks.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

static void prv_period_fn(struct pbl_work *work);

// Guards what follows: references come from apps and from more than one work queue.
static PBL_MUTEX_DEFINE(s_lock);
static unsigned int s_refcount;
// The references of the period and of having no bonding, held at most once each.
static bool s_period_ref;
static bool s_unbonded_ref;
static uint64_t s_period_end_ticks;

static PBL_DELAYED_WORK_DEFINE(s_period_work, prv_period_fn);

// Only SysWorkq touches it.
static bool s_discoverable;

static void prv_evaluate(void *data) {
  bool pairable;

  PBL_ASSERT_TASK(PebbleTask_SysWorkq);

  if (!bt_ctl_is_bluetooth_running()) {
    return;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  pairable = (s_refcount > 0U);
  pbl_mutex_unlock(&s_lock);

  PBL_LOG_DBG("Pairable: %d", (int)pairable);

  pbl_bt_le_pairability_set_enabled(pairable);
  if (s_discoverable != pairable) {
    bt_advertising_set_discoverable(pairable);
    s_discoverable = pairable;
  }
}

// Always evaluate, even if the state might not have changed: this pretty much only happens in
// response to user input, and skipping evaluations has caused bugs before.
static void prv_schedule_evaluation(void) {
  pbl_sys_work_call(prv_evaluate, nullptr);
}

void bt_pairability_use(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_refcount++;
  pbl_mutex_unlock(&s_lock);

  prv_schedule_evaluation();
}

void bt_pairability_release(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  PBL_ASSERTN(s_refcount != 0U);
  s_refcount--;
  pbl_mutex_unlock(&s_lock);

  prv_schedule_evaluation();
}

static void prv_period_fn(struct pbl_work *work) {
  uint64_t now = pbl_uptime_ticks();

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_period_ref) {
    if (now < s_period_end_ticks) {
      // The period was extended after this was queued.
      pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_period_work,
                                  PBL_TICKS(s_period_end_ticks - now));
    } else {
      s_period_ref = false;
      s_refcount--;
    }
  }
  pbl_mutex_unlock(&s_lock);

  prv_schedule_evaluation();
}

void bt_pairability_use_for_period(uint16_t duration_secs) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_period_ref || duration_secs != 0U) {
    if (!s_period_ref) {
      s_period_ref = true;
      s_refcount++;
    }

    s_period_end_ticks = pbl_uptime_ticks() + pbl_ms_to_ticks(duration_secs * 1000U);
    pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_period_work,
                                PBL_MSEC(duration_secs * 1000U));
  }
  pbl_mutex_unlock(&s_lock);

  prv_schedule_evaluation();
}

void bt_pairability_update_due_to_bonding_change(void) {
  const bool unbonded = (pbl_bt_bond_count() == 0);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (unbonded != s_unbonded_ref) {
    s_unbonded_ref = unbonded;
    if (unbonded) {
      s_refcount++;
    } else {
      s_refcount--;
    }
  }
  pbl_mutex_unlock(&s_lock);

  prv_schedule_evaluation();
}

static void prv_init(void *data) {
  // Advertising was torn down before this, so it has to be told again.
  s_discoverable = false;
  prv_evaluate(nullptr);
}

void bt_pairability_init(void) {
  bt_pairability_update_due_to_bonding_change();
  pbl_sys_work_call(prv_init, nullptr);
}
