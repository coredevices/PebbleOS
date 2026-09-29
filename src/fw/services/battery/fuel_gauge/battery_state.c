/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <math.h>

#include "board/board.h"
#include <pbl/fuel_gauge/fuel_gauge.h>
#include <pbl/drivers/battery.h>
#include <pbl/drivers/pmic.h>
#include <pbl/drivers/pmic/npm1300.h>
#include <pbl/drivers/rtc.h>
#include "kernel/events.h"
#include "pbl/services/analytics/analytics.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/system_task.h"
#include "syscall/syscall_internal.h"
#include <pbl/logging/logging.h>
#include "system/passert.h"
#include "util/ratio.h"

#ifndef CONFIG_RECOVERY_FW
#include "pbl/services/settings/settings_file.h"
#endif

#ifdef CONFIG_MFG
#include <pbl/drivers/flash.h>
#include "flash_region/flash_region.h"
#endif

PBL_LOG_MODULE_DECLARE(service_battery, CONFIG_SERVICE_BATTERY_LOG_LEVEL);

#if !defined(CONFIG_RECOVERY_FW) || defined(CONFIG_MFG)
#define FUEL_GAUGE_STATEFUL 1
#else
#define FUEL_GAUGE_STATEFUL 0
#endif

#define ALWAYS_UPDATE_PCT     10.0f
#define RECONNECTION_DELAY_MS (1 * 1000)
// TODO: Adjust sample rate based on activity periods once we have good
// power consumption profiles
#define BATTERY_SAMPLE_RATE_MIN 1

#define LOG_MIN_SEC 30

// ADC resolution (~5 mV) plus cell model error.
#define VOLTAGE_NOISE_V 0.015f
// One current sample per minute stands in for the average of a bursty load.
#define CURRENT_NOISE_A 0.005f

extern const struct pbl_fuel_gauge_model battery_model;

static struct pbl_fuel_gauge_config s_fg_config = {
  .model = &battery_model,
  .voltage_noise = VOLTAGE_NOISE_V,
  .current_noise = CURRENT_NOISE_A,
  .term_voltage = NPM1300_TERM_VOLTAGE_MV / 1000.0f,
};
static struct pbl_fuel_gauge s_fg;

static PreciseBatteryChargeState s_last_battery_charge_state;
static TimerID s_periodic_timer_id = TIMER_INVALID_ID;

static volatile bool s_update_pending = false;
static volatile bool s_pending_force_update = false;

static BatteryChargeStatus s_last_chg_status;
static RtcTicks prv_ref_time;
static int32_t s_last_voltage_mv;
static int32_t s_last_temp_mc;
static uint32_t s_last_soc_cpct;
static uint32_t s_soc_cpct_min = UINT32_MAX;
static int32_t s_analytics_last_voltage_mv;
static uint32_t s_analytics_last_cpct;

//! Track the lowest SOC seen since the last heartbeat: hourly snapshots miss
//! brief deep discharges, which matter when correlating battery behavior with
//! brownout-cleared states.
static void prv_track_soc_min(void) {
  if (s_last_soc_cpct < s_soc_cpct_min) {
    s_soc_cpct_min = s_last_soc_cpct;
  }
}
static uint32_t s_last_tte;
static uint32_t s_last_ttf;
static RtcTicks s_last_log;
static bool s_charger_enabled;

#if FUEL_GAUGE_STATEFUL
#define FUEL_GAUGE_SAVE_INTERVAL_S 300

static uint32_t s_save_counter;

#ifdef CONFIG_MFG
// In manufacturing firmware, use dedicated MFG_BATTERY_STATE flash region
static void prv_erase_state(void) {
  flash_erase_subsector_blocking(FLASH_REGION_MFG_BATTERY_STATE_BEGIN);
  PBL_LOG_DBG("Fuel gauge state erased");
}

static bool prv_load_state(void *state, size_t size) {
  if (size > (FLASH_REGION_MFG_BATTERY_STATE_END - FLASH_REGION_MFG_BATTERY_STATE_BEGIN)) {
    return false;
  }

  flash_read_bytes(state, FLASH_REGION_MFG_BATTERY_STATE_BEGIN, size);

  // Check if the flash region contains valid data (not all 0xFF)
  uint8_t *bytes = (uint8_t *)state;
  bool all_erased = true;
  for (size_t i = 0; i < size; i++) {
    if (bytes[i] != 0xFF) {
      all_erased = false;
      break;
    }
  }

  if (all_erased) {
    return false;
  }

  PBL_LOG_DBG("Fuel gauge state loaded");

  return true;
}

static void prv_save_state(void) {
  struct pbl_fuel_gauge_state buf;

  pbl_fuel_gauge_state_get(&s_fg, &buf);

  if (sizeof(buf) > (FLASH_REGION_MFG_BATTERY_STATE_END - FLASH_REGION_MFG_BATTERY_STATE_BEGIN)) {
    PBL_LOG_ERR("Fuel gauge state too large for MFG_BATTERY_STATE region");
    return;
  }

  flash_erase_subsector_blocking(FLASH_REGION_MFG_BATTERY_STATE_BEGIN);
  flash_write_bytes((const uint8_t *)&buf, FLASH_REGION_MFG_BATTERY_STATE_BEGIN, sizeof(buf));

  PBL_LOG_DBG("Fuel gauge state saved");
}
#else
// In normal firmware, use settings file
#define FUEL_GAUGE_SETTINGS_FILE_NAME "fgs"
#define FUEL_GAUGE_SETTINGS_MAX_SIZE  2048

static const uint32_t FUEL_GAUGE_STATE_KEY = 1;

static void prv_erase_state(void) {
  SettingsFile file;
  status_t ret;

  ret = settings_file_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE);
  if (ret != S_SUCCESS) {
    return;
  }

  settings_file_delete(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY));
  settings_file_close(&file);

  PBL_LOG_DBG("Fuel gauge state erased");
}

static bool prv_load_state(void *state, size_t size) {
  SettingsFile file;
  status_t ret;

  ret = settings_file_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE);
  if (ret != S_SUCCESS) {
    return false;
  }

  int len = settings_file_get_len(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY));
  if (len != (int)size) {
    settings_file_close(&file);
    return false;
  }

  ret = settings_file_get(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY), state, size);
  settings_file_close(&file);

  if (ret != S_SUCCESS) {
    return false;
  }

  PBL_LOG_DBG("Fuel gauge state loaded");

  return true;
}

static void prv_save_state(void) {
  struct pbl_fuel_gauge_state buf;
  SettingsFile file;
  status_t ret;

  pbl_fuel_gauge_state_get(&s_fg, &buf);

  ret = settings_file_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME, FUEL_GAUGE_SETTINGS_MAX_SIZE);
  if (ret != S_SUCCESS) {
    PBL_LOG_ERR("Failed to open fuel gauge settings file");
    return;
  }

  ret = settings_file_set(&file, &FUEL_GAUGE_STATE_KEY, sizeof(FUEL_GAUGE_STATE_KEY), &buf,
                          sizeof(buf));
  settings_file_close(&file);

  if (ret != S_SUCCESS) {
    PBL_LOG_ERR("Failed to save fuel gauge state");
  } else {
    PBL_LOG_DBG("Fuel gauge state saved");
  }
}
#endif // CONFIG_MFG
#endif // FUEL_GAUGE_STATEFUL

static void prv_schedule_update(uint32_t delay, bool force_update);

static struct pbl_fuel_gauge_meas prv_meas(const BatteryConstants *constants) {
  return (struct pbl_fuel_gauge_meas){
    .v = (float)constants->v_mv / 1000.0f,
    .i = (float)constants->i_ua / 1000000.0f,
    .t = (float)constants->t_mc / 1000.0f,
  };
}

static void prv_fuel_gauge_init(const BatteryConstants *constants) {
  const struct pbl_fuel_gauge_meas meas = prv_meas(constants);
  const struct pbl_fuel_gauge_state *saved = NULL;
  int ret;

#if FUEL_GAUGE_STATEFUL
  struct pbl_fuel_gauge_state state;

  if (prv_load_state(&state, sizeof(state))) {
    saved = &state;
  }
#endif

  ret = pbl_fuel_gauge_init(&s_fg, &s_fg_config, &meas, saved);
  PBL_ASSERTN(ret >= 0);

#if FUEL_GAUGE_STATEFUL
  if ((saved != NULL) && (ret == 0)) {
    PBL_LOG_WRN("Saved fuel gauge state does not match the battery, erasing");
    prv_erase_state();
  }
#endif
}

static enum pbl_fuel_gauge_charge_state prv_charge_state(bool is_plugged,
                                                         BatteryChargeStatus chg_status) {
  if (!is_plugged) {
    return PBL_FUEL_GAUGE_DISCHARGING;
  }

  switch (chg_status) {
    case BatteryChargeStatusComplete:
      return PBL_FUEL_GAUGE_CHARGE_COMPLETE;
    case BatteryChargeStatusTrickle:
    case BatteryChargeStatusCC:
      return PBL_FUEL_GAUGE_CHARGING_CC;
    case BatteryChargeStatusCV:
      return PBL_FUEL_GAUGE_CHARGING_CV;
    default:
      return PBL_FUEL_GAUGE_DISCHARGING;
  }
}

static void prv_battery_state_put_change_event(PreciseBatteryChargeState state) {
  PebbleEvent e = {
    .type = PEBBLE_BATTERY_STATE_CHANGE_EVENT,
    .battery_state = {
      .new_state = state,
    },
  };
  event_put(&e);
}

static void prv_update_state(void *force_update) {
  enum pbl_fuel_gauge_charge_state fg_state;
  BatteryChargeStatus chg_status;
  BatteryConstants constants;
  RtcTicks now;
  float delta;
  uint8_t pct_int;
  bool is_plugged;
  bool is_charging;
  bool update;
  float pct;
  int ret;

  s_update_pending = false;
  update = (force_update != NULL) || s_pending_force_update;
  s_pending_force_update = false;

  ret = battery_get_constants(&constants);
  if (ret < 0) {
    PBL_LOG_ERR("Could not obtain constants, skipping update (%d)", ret);
    return;
  }

  is_plugged = battery_is_usb_connected_impl();
  if (is_plugged != s_last_battery_charge_state.is_plugged) {
    s_last_battery_charge_state.is_plugged = is_plugged;
    if (is_plugged) {
      PBL_ANALYTICS_TIMER_STOP(battery_discharge_duration_ms);
    } else {
      PBL_ANALYTICS_TIMER_START(battery_discharge_duration_ms);
    }
    update = true;
  }

  ret = battery_charge_status_get(&chg_status);
  if (ret < 0) {
    PBL_LOG_ERR("Could not obtain charge status, skipping update (%d)", ret);
    return;
  }

  s_last_chg_status = chg_status;

  is_charging = is_plugged && !(chg_status == BatteryChargeStatusComplete ||
                                chg_status == BatteryChargeStatusUnknown);
  if (is_charging != s_last_battery_charge_state.is_charging) {
    s_last_battery_charge_state.is_charging = is_charging;
    if (is_charging) {
      PBL_ANALYTICS_TIMER_START(battery_charge_time_ms);
    } else {
      PBL_ANALYTICS_TIMER_STOP(battery_charge_time_ms);
    }
    update = true;
  }

  s_last_voltage_mv = constants.v_mv;
  s_last_temp_mc = constants.t_mc;

  now = rtc_get_ticks();
  delta = (float)(now - prv_ref_time) / (float)RTC_TICKS_HZ;
  prv_ref_time = now;

  const struct pbl_fuel_gauge_meas meas = prv_meas(&constants);
  fg_state = prv_charge_state(is_plugged, chg_status);
  pct = pbl_fuel_gauge_update(&s_fg, &meas, delta, fg_state);

  pct_int = (uint8_t)ceilf(pct);
  s_last_soc_cpct = (uint32_t)(pct * 100.0f);
  prv_track_soc_min();
  if (pct_int != s_last_battery_charge_state.pct) {
    s_last_battery_charge_state.pct = pct_int;
    s_last_battery_charge_state.charge_percent = (uint32_t)(pct * RATIO32_MAX) / 100U;
    update = true;
  }

  if (s_last_battery_charge_state.is_charging) {
    pbl_fuel_gauge_ttf_get(&s_fg, &s_last_ttf);
    s_last_tte = 0U;
  } else {
    pbl_fuel_gauge_tte_get(&s_fg, &s_last_tte);
    s_last_ttf = 0U;
  }

#if FUEL_GAUGE_STATEFUL
  if (update || (++s_save_counter >= FUEL_GAUGE_SAVE_INTERVAL_S)) {
    s_save_counter = 0;
    prv_save_state();
  }
#endif

  PBL_LOG_VERBOSE(
      "Battery state: v_mv: %ld, i_ua: %ld, t_mc: %ld, td_ms: %lu, fg: %d, soc: %u, tte: %lu, "
      "ttf: %lu",
      constants.v_mv, constants.i_ua, constants.t_mc, (uint32_t)(delta * 1000.0f), (int)fg_state,
      s_last_battery_charge_state.pct, s_last_tte, s_last_ttf);

  if (update || (((now - s_last_log) / RTC_TICKS_HZ > LOG_MIN_SEC) &&
                 (s_last_battery_charge_state.is_charging || (pct < ALWAYS_UPDATE_PCT)))) {
    PBL_LOG_INFO("Percent: %" PRIu8 ", V: %" PRId32 " mV, I: %" PRId32
                 " uA, "
                 "T: %" PRId32 " mC, charging: %s, plugged: %s",
                 s_last_battery_charge_state.pct, constants.v_mv, constants.i_ua, constants.t_mc,
                 s_last_battery_charge_state.is_charging ? "yes" : "no",
                 s_last_battery_charge_state.is_plugged ? "yes" : "no");
    prv_battery_state_put_change_event(s_last_battery_charge_state);
    s_last_log = now;
  }

  // Enable battery charging after fuel gauge state has been updated for the first time
  if (!s_charger_enabled) {
    s_charger_enabled = true;
    pmic_set_charger_state(true);
  }
}

static void prv_enqueue_update(bool force) {
  if (force) {
    s_pending_force_update = true;
  }
  if (s_update_pending) {
    return;
  }
  if (system_task_add_callback(prv_update_state, NULL)) {
    s_update_pending = true;
  }
}

static void prv_update_callback(void *data) {
  new_timer_stop(s_periodic_timer_id);
  prv_enqueue_update(data != NULL);
}

static void prv_callback_from_regular_timer(void *data) {
  // no need to stop the new_timer here, since this came from the regular_timer
  prv_enqueue_update(false);
}

static void prv_schedule_update(uint32_t delay, bool force_update) {
  bool success = new_timer_start(s_periodic_timer_id, delay, prv_update_callback,
                                 (void *)force_update, 0 /*flags*/);
  PBL_ASSERTN(success);
}

void battery_state_force_update(void) {
  prv_schedule_update(0, true);
}

void battery_state_init(void) {
  BatteryConstants constants;
  int ret;

  ret = battery_get_constants(&constants);
  PBL_ASSERTN(ret == 0);

  s_last_voltage_mv = constants.v_mv;

  s_fg_config.term_current =
      (float)(NPM1300_CONFIG.chg_current_ma * NPM1300_CONFIG.term_current_pct / 100U) / 1000.0f;
  prv_fuel_gauge_init(&constants);

  ret = battery_charge_status_get(&s_last_chg_status);
  PBL_ASSERTN(ret == 0);

  s_last_battery_charge_state.is_plugged = battery_is_usb_connected_impl();
  s_last_battery_charge_state.is_charging = s_last_battery_charge_state.is_plugged &&
                                            !(s_last_chg_status == BatteryChargeStatusComplete ||
                                              s_last_chg_status == BatteryChargeStatusUnknown);

  const struct pbl_fuel_gauge_meas meas = prv_meas(&constants);
  float pct = pbl_fuel_gauge_update(
      &s_fg, &meas, 0.0f,
      prv_charge_state(s_last_battery_charge_state.is_plugged, s_last_chg_status));

  prv_ref_time = rtc_get_ticks();

  s_last_soc_cpct = (uint32_t)(pct * 100.0f);
  prv_track_soc_min();
  s_last_battery_charge_state.pct = (uint8_t)ceilf(pct);
  s_last_battery_charge_state.charge_percent = (uint32_t)(pct * RATIO32_MAX) / 100U;

  if (s_last_battery_charge_state.is_charging) {
    PBL_ANALYTICS_TIMER_START(battery_charge_time_ms);
  } else if (!s_last_battery_charge_state.is_plugged) {
    PBL_ANALYTICS_TIMER_START(battery_discharge_duration_ms);
  }

  s_periodic_timer_id = new_timer_create();

  battery_state_force_update();

  static RegularTimerInfo battery_regular_timer = {.cb = prv_callback_from_regular_timer};
  regular_timer_add_multiminute_callback(&battery_regular_timer, BATTERY_SAMPLE_RATE_MIN);

  s_analytics_last_voltage_mv = s_last_voltage_mv;
  s_analytics_last_cpct = s_last_soc_cpct;
}

void battery_state_handle_connection_event(bool is_connected) {
  prv_schedule_update(RECONNECTION_DELAY_MS, true);
}

DEFINE_SYSCALL(BatteryChargeState, sys_battery_get_charge_state, void) {
  return battery_get_charge_state();
}

BatteryChargeState battery_get_charge_state(void) {
  BatteryChargeState state;

  state.charge_percent = s_last_battery_charge_state.pct;
  state.is_charging = s_last_battery_charge_state.is_charging;
  state.is_plugged = s_last_battery_charge_state.is_plugged;

  return state;
}

// For unit tests
TimerID battery_state_get_periodic_timer_id(void) {
  return s_periodic_timer_id;
}

uint16_t battery_state_get_voltage(void) {
  return (uint16_t)s_last_voltage_mv;
}

int32_t battery_state_get_temperature(void) {
  return s_last_temp_mc;
}

#include "console/prompt.h"
void command_print_battery_status(void) {
  char buffer[32];

  prompt_send_response_fmt(buffer, 32, "%" PRId32 " mV", s_last_voltage_mv);
  prompt_send_response_fmt(buffer, 32, "soc: %" PRIu8 "%% (%" PRIu32 ")",
                           s_last_battery_charge_state.pct,
                           s_last_battery_charge_state.charge_percent);
  if (s_last_tte == 0U) {
    prompt_send_response_fmt(buffer, 32, "tte: N/A");
  } else {
    prompt_send_response_fmt(buffer, 32, "tte: %" PRIu32 "s", s_last_tte);
  }
  if (s_last_ttf == 0U) {
    prompt_send_response_fmt(buffer, 32, "ttf: N/A");
  } else {
    prompt_send_response_fmt(buffer, 32, "ttf: %" PRIu32 "s", s_last_ttf);
  }
  prompt_send_response_fmt(buffer, 32, "plugged: %s",
                           s_last_battery_charge_state.is_plugged ? "YES" : "NO");
  prompt_send_response_fmt(buffer, 32, "charging: %s",
                           s_last_battery_charge_state.is_charging ? "YES" : "NO");
}

/////////////////
// Analytics

// Note that this is run on a different thread than battery_state!
void pbl_analytics_external_collect_battery(void) {
  int32_t battery_mv = s_last_voltage_mv;
  uint32_t battery_soc_cpct = s_last_soc_cpct;
  int32_t d_mv;
  uint32_t d_soc_cpct;

  d_mv = battery_mv - s_analytics_last_voltage_mv;
  PBL_ANALYTICS_SET_UNSIGNED(battery_voltage, battery_mv);
  PBL_ANALYTICS_SET_SIGNED(battery_voltage_delta, d_mv);
  PBL_ANALYTICS_SET_SIGNED(battery_temp_c, s_last_temp_mc);
  PBL_ANALYTICS_SET_UNSIGNED(battery_soc_pct_min,
                             s_soc_cpct_min < battery_soc_cpct ? s_soc_cpct_min : battery_soc_cpct);
  s_soc_cpct_min = battery_soc_cpct;
  s_analytics_last_voltage_mv = battery_mv;

  d_soc_cpct = MAX((int32_t)s_analytics_last_cpct - (int32_t)battery_soc_cpct, 0);
  PBL_ANALYTICS_SET_UNSIGNED(battery_soc_pct, battery_soc_cpct);
  PBL_ANALYTICS_SET_UNSIGNED(battery_soc_pct_drop, d_soc_cpct);
  s_analytics_last_cpct = battery_soc_cpct;

  PBL_ANALYTICS_SET_UNSIGNED(battery_tte_s, s_last_tte);
}

static void prv_set_forced_charge_state(bool is_charging) {
  battery_force_charge_enable(is_charging);

  // Trigger an immediate update to the state machine: may trigger an event
  battery_state_force_update();
}

void command_battery_charge_option(const char *option) {
  if (!strcmp("disable", option)) {
    prv_set_forced_charge_state(false);
  } else if (!strcmp("enable", option)) {
    prv_set_forced_charge_state(true);
  }
}
