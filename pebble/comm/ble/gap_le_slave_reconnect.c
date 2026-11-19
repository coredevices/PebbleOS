/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_slave_reconnect.h"

#include "gap_le_advert.h"
#include "gap_le_connect.h"

#include <pbl/bluetooth/bond.h>
#include <pbl/btutil/bt_uuid.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>

#include <applib/bluetooth/ble_ad_parse.h>
#include <comm/bt_lock.h>
#include <kernel/event_loop.h>

//! Reference to the reconnection advertising job.
//! bt_lock() needs to be taken before accessing this variable.
static GAPLEAdvertisingJobRef s_reconnect_advert_job;
static bool s_is_basic_reconnection_enabled;

typedef enum {
  ReconnectType_None,  // Not advertising for reconnection
  ReconnectType_Plain, // Advertising for reconnection with empty payload
} ReconnectType;

//! When the master reconnects and drops repeatedly (e.g. supervision timeouts
//! in bad RF conditions), skip the initial short-interval advertising term to
//! bound the radio duty cycle. The window resets after
//! RECONNECT_CHURN_WINDOW_SECS, so during sustained churn one attempt per
//! window still gets the fast short-interval burst.
#define RECONNECT_CHURN_WINDOW_SECS (60)
#define RECONNECT_CHURN_THRESHOLD   (3)

//! bt_lock() needs to be taken before accessing these variables.
static uint64_t s_churn_window_start_ticks;
static uint32_t s_churn_count;

static bool prv_should_skip_short_interval(void) {
  const uint64_t now = pbl_uptime_ticks();
  if ((now - s_churn_window_start_ticks) > (RECONNECT_CHURN_WINDOW_SECS * PBL_TICK_HZ)) {
    s_churn_window_start_ticks = now;
    s_churn_count = 0;
  }
  ++s_churn_count;
  return (s_churn_count > RECONNECT_CHURN_THRESHOLD);
}

// -----------------------------------------------------------------------------
//! Static, internal helper functions
static void prv_advert_job_unscheduled_callback(GAPLEAdvertisingJobRef job, bool completed,
                                                void *data) {
  // bt_lock() is still held for us by gap_le_advert
  s_reconnect_advert_job = nullptr;
}

static bool prv_is_advertising_for_reconnection(void) {
  return (s_reconnect_advert_job != nullptr);
}

static ReconnectType prv_current_reconnect_type(void) {
  if (s_is_basic_reconnection_enabled) {
    return ReconnectType_Plain;
  }
  return ReconnectType_None;
}

static void prv_unschedule_adv_if_needed(void) {
  if (prv_is_advertising_for_reconnection()) {
    gap_le_advert_unschedule(s_reconnect_advert_job);
  }
}

static void prv_evaluate(ReconnectType prev_type) {
  ReconnectType cur_type = prv_current_reconnect_type();
  if (cur_type == prev_type) {
    PBL_LOG_DBG("Reconnect type unchanged: %d", cur_type);
    return;
  }

  PBL_LOG_DBG("Reconnect type changed: %d -> %d", prev_type, cur_type);

  if (cur_type != ReconnectType_None) {
    prv_unschedule_adv_if_needed();

    // [MT] Advertise with an empty payload to save battery life with these
    // reconnection ad packets. This should be enough for the other
    // device to be able to reconnect. With iOS it works, need to test Android.

    // [MT] Note we leave out the Flags AD. According to the spec you have to
    // include flags if any are non-zero. To abide, Pebble ought to always
    // include the SIMULTANEOUS_LE_BR_EDR_TO_SAME_DEVICE_CONTROLLER and
    // SIMULTANEOUS_LE_BR_EDR_TO_SAME_DEVICE_HOST flags. However, we have never
    // done this (ignorance) and gotten by, by using a "random" address (the
    // public address, but then inverted) as a work-around for the problems
    // leaving out these flags caused with Android.
    // I intend to use use the "Peripheral privacy feature" some time in the
    // near future. With this, these flags and the issues on Android become
    // a non-issue (because addresses will be private). Therefore I decided to
    // still leave out the flags.

    static struct pbl_bt_ad_data payload = {
      .ad_data_length = 0,
      .scan_resp_data_length = 0,
    };

    // Values chosen according to Apple Accessory Design Guidelines
    const GAPLEAdvertisingJobTerm advert_terms[] = {
      {
        .duration_secs = 30,
        .interval = GAPLEAdvertisingInterval_Short,
      },
      {
        .duration_secs = GAPLE_ADVERTISING_DURATION_INFINITE,
        .interval = GAPLEAdvertisingInterval_Long,
      },
    };

    const GAPLEAdvertisingJobTerm *terms = advert_terms;
    uint8_t num_terms = ARRAY_LENGTH(advert_terms);
    if (prv_should_skip_short_interval()) {
      PBL_LOG_WRN("Reconnect churn: advertising at long interval only");
      terms = &advert_terms[1];
      num_terms = 1;
    }

    s_reconnect_advert_job =
        gap_le_advert_schedule(&payload, terms, num_terms, prv_advert_job_unscheduled_callback,
                               nullptr, GAPLEAdvertisingJobTagReconnection);
  } else {
    prv_unschedule_adv_if_needed();
  }
}

static void prv_set_and_evaluate(bool *val, bool new_value) {
  const ReconnectType prev_type = prv_current_reconnect_type();
  *val = new_value;
  prv_evaluate(prev_type);
}

// -----------------------------------------------------------------------------
void gap_le_slave_reconnect_stop(void) {
  bt_lock();
  {
    prv_set_and_evaluate(&s_is_basic_reconnection_enabled, false);
  }
  bt_unlock();
}

// -----------------------------------------------------------------------------
void gap_le_slave_reconnect_start(void) {
#ifdef CONFIG_RECOVERY_FW
  return; // Only use discoverable packet for PRF
#endif
  bt_lock();
  {
    if (prv_is_advertising_for_reconnection()) {
      PBL_LOG_DBG("Already advertising for reconnection");
      goto unlock;
    }

    if (gap_le_connect_is_connected_as_slave()) {
      PBL_LOG_DBG("Already connected as slave");
      goto unlock;
    }

    if (pbl_bt_bond_count() == 0) {
      PBL_LOG_DBG("No bonded master device");
      goto unlock;
    }

    prv_set_and_evaluate(&s_is_basic_reconnection_enabled, true);
  }
unlock:
  bt_unlock();
}
