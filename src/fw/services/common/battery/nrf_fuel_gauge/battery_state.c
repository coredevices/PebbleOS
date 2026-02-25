/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <math.h>

#include "board/board.h"
#include "drivers/battery.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "services/common/analytics/analytics.h"
#include "services/common/battery/battery_state.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/system_task.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"
#include "util/ratio.h"

#ifndef RECOVERY_FW
#include "services/normal/settings/settings_file.h"
#endif

#include "nrf_fuel_gauge.h"

#define ALWAYS_UPDATE_PCT 10.0f
#define RECONNECTION_DELAY_MS (1 * 1000)
// TODO: Adjust sample rate based on activity periods once we have good
// power consumption profiles
#define BATTERY_SAMPLE_RATE_S 1
// Use fake current data for the first N seconds to improve model accuracy
#define FAKE_CURRENT_DURATION_S 10
#define FAKE_CURRENT_UA 100

#define LOG_MIN_SEC 30

static const struct battery_model prv_battery_model = {
#if PLATFORM_ASTERIX
#include "battery_asterix.inc"
#elif PLATFORM_OBELIX
#include "battery_obelix.inc"
#elif PLATFORM_GETAFIX
#include "battery_getafix.inc"
#else
#error "Battery model not defined for this platform"
#endif
};

static PreciseBatteryChargeState s_last_battery_charge_state;
static TimerID s_periodic_timer_id = TIMER_INVALID_ID;

static BatteryChargeStatus s_last_chg_status;
static uint64_t prv_ref_time;
static uint32_t s_sample_count;
static int32_t s_last_voltage_mv;
static int32_t s_last_temp_mc;
static int32_t s_analytics_last_voltage_mv;
static uint8_t s_analytics_last_pct;
static uint32_t s_last_tte;
static uint32_t s_last_ttf;
static RtcTicks s_last_log;

#ifndef RECOVERY_FW
#define FUEL_GAUGE_SETTINGS_FILE_NAME "fgs"
#define FUEL_GAUGE_SAVE_INTERVAL_S 300
#define FUEL_GAUGE_SETTINGS_MAX_SIZE 512

static const uint32_t FUEL_GAUGE_STATE_KEY = 1;
static uint32_t s_save_counter;

static bool prv_load_state(void *state, size_t size) {
  SettingsFile file;
  status_t ret;

  ret = settings_file_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME,
                           FUEL_GAUGE_SETTINGS_MAX_SIZE);
  if (ret != S_SUCCESS) {
    return false;
  }

  int len = settings_file_get_len(&file, &FUEL_GAUGE_STATE_KEY,
                                  sizeof(FUEL_GAUGE_STATE_KEY));
  if (len != (int)size) {
    settings_file_close(&file);
    return false;
  }

  ret = settings_file_get(&file, &FUEL_GAUGE_STATE_KEY,
                          sizeof(FUEL_GAUGE_STATE_KEY), state, size);
  settings_file_close(&file);

  return (ret == S_SUCCESS);
}

static void prv_save_state(void) {
  uint8_t buf[nrf_fuel_gauge_state_size];
  SettingsFile file;
  status_t ret;

  if (nrf_fuel_gauge_state_get(buf, sizeof(buf)) != 0) {
    PBL_LOG_ERR("Failed to get fuel gauge state");
    return;
  }

  ret = settings_file_open(&file, FUEL_GAUGE_SETTINGS_FILE_NAME,
                           FUEL_GAUGE_SETTINGS_MAX_SIZE);
  if (ret != S_SUCCESS) {
    PBL_LOG_ERR("Failed to open fuel gauge settings file");
    return;
  }

  ret = settings_file_set(&file, &FUEL_GAUGE_STATE_KEY,
                          sizeof(FUEL_GAUGE_STATE_KEY), buf, sizeof(buf));
  settings_file_close(&file);

  if (ret != S_SUCCESS) {
    PBL_LOG_ERR("Failed to save fuel gauge state");
  }
}
#endif

static void prv_schedule_update(uint32_t delay, bool force_update);

static void prv_charge_status_inform(BatteryChargeStatus chg_status) {
  union nrf_fuel_gauge_ext_state_info_data state_info;
  int ret;

  switch (chg_status) {
    case BatteryChargeStatusComplete:
      state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_COMPLETE;
      break;
    case BatteryChargeStatusTrickle:
      state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_TRICKLE;
      break;
    case BatteryChargeStatusCC:
      state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_CC;
      break;
    case BatteryChargeStatusCV:
      state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_CV;
      break;
    default:
      state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_IDLE;
      break;
  }

  ret = nrf_fuel_gauge_ext_state_update(NRF_FUEL_GAUGE_EXT_STATE_INFO_CHARGE_STATE_CHANGE,
                                        &state_info);
  PBL_ASSERTN(ret == 0);
}

static void prv_battery_state_put_change_event(PreciseBatteryChargeState state) {
  PebbleEvent e = {
      .type = PEBBLE_BATTERY_STATE_CHANGE_EVENT,
      .battery_state =
          {
              .new_state = state,
          },
  };
  event_put(&e);
}

static void prv_update_state(void *force_update) {
  BatteryChargeStatus chg_status;
  BatteryConstants constants;
  RtcTicks now, delta;
  uint8_t pct_int;
  bool is_plugged;
  bool is_charging;
  bool update;
  float pct;
  int ret;

  update = force_update != NULL;

  is_plugged = battery_is_usb_connected_impl();
  if (is_plugged != s_last_battery_charge_state.is_plugged) {
    ret = nrf_fuel_gauge_ext_state_update(is_plugged
                                              ? NRF_FUEL_GAUGE_EXT_STATE_INFO_VBUS_CONNECTED
                                              : NRF_FUEL_GAUGE_EXT_STATE_INFO_VBUS_DISCONNECTED,
                                          NULL);
    PBL_ASSERTN(ret == 0);
    s_last_battery_charge_state.is_plugged = is_plugged;
    update = true;
  }

  ret = battery_charge_status_get(&chg_status);
  if (ret < 0) {
    PBL_LOG_ERR("Could not obtain charge status, skipping update (%d)", ret);
    return;
  }

  if (chg_status != s_last_chg_status) {
    s_last_chg_status = chg_status;
    prv_charge_status_inform(chg_status);
  }

  is_charging = is_plugged && !(chg_status == BatteryChargeStatusComplete ||
                                chg_status == BatteryChargeStatusUnknown);
  if (is_charging != s_last_battery_charge_state.is_charging) {
    s_last_battery_charge_state.is_charging = is_charging;
    update = true;
  }

  ret = battery_get_constants(&constants);
  if (ret < 0) {
    PBL_LOG_ERR("Could not obtain constants, skipping update (%d)", ret);
    return;
  }

  s_last_voltage_mv = constants.v_mv;
  s_last_temp_mc = constants.t_mc;

  now = rtc_get_ticks();
  delta = (now - prv_ref_time) / RTC_TICKS_HZ;
  prv_ref_time = now;

  // Use fake current data for the first N seconds to improve model accuracy
  int32_t current_ua = constants.i_ua;
  if (s_sample_count < DIVIDE_CEIL(FAKE_CURRENT_DURATION_S, BATTERY_SAMPLE_RATE_S)) {
    current_ua = FAKE_CURRENT_UA;
    s_sample_count++;
  }

  pct = nrf_fuel_gauge_process((float)constants.v_mv / 1000.0f, (float)current_ua / 1000000.0f,
                               (float)constants.t_mc / 1000.0f, (float)delta, NULL);

  pct_int = (uint8_t)ceilf(pct);
  if (pct_int != s_last_battery_charge_state.pct) {
    s_last_battery_charge_state.pct = pct_int;
    s_last_battery_charge_state.charge_percent = (uint32_t)(pct * RATIO32_MAX) / 100U;
    update = true;
  }

  if (s_last_battery_charge_state.is_charging) {
    float ttf;

    ttf = nrf_fuel_gauge_ttf_get();
    if (!isnanf(ttf)) {
      s_last_ttf = (uint32_t)ttf;
    }

    s_last_tte = 0U;
  } else {
    float tte;

    tte = nrf_fuel_gauge_tte_get();
    if (!isnanf(tte)) {
      s_last_tte = (uint32_t)tte;
    }

    s_last_ttf = 0U;
  }

#ifndef RECOVERY_FW
  if (++s_save_counter >= FUEL_GAUGE_SAVE_INTERVAL_S) {
    s_save_counter = 0;
    prv_save_state();
  }
#endif

  PBL_LOG_VERBOSE("Battery state: v_mv: %ld, i_ua: %ld, t_mc: %ld, td: %lu, soc: %u, tte: %lu, ttf: %lu",
          constants.v_mv, constants.i_ua, constants.t_mc, (uint32_t)delta,
          s_last_battery_charge_state.pct, s_last_tte, s_last_ttf);

  if (update || (((now - s_last_log) / RTC_TICKS_HZ > LOG_MIN_SEC) &&
                 (s_last_battery_charge_state.is_charging || (pct < ALWAYS_UPDATE_PCT)))) {
    PBL_LOG_INFO("Percent: %" PRIu8 ", V: %" PRId32 " mV, I: %" PRId32 " uA, "
            "T: %" PRId32 " mC, charging: %s, plugged: %s",
            s_last_battery_charge_state.pct, constants.v_mv, constants.i_ua, constants.t_mc,
            s_last_battery_charge_state.is_charging ? "yes" : "no",
            s_last_battery_charge_state.is_plugged ? "yes" : "no");
    prv_battery_state_put_change_event(s_last_battery_charge_state);
    s_last_log = now;
  }
}

static void prv_update_callback(void *data) {
  new_timer_stop(s_periodic_timer_id);
  system_task_add_callback(prv_update_state, data);
}

static void prv_callback_from_regular_timer(void *data) {
  // no need to stop the new_timer here, since this came from the regular_timer
  system_task_add_callback(prv_update_state, data);
}

static void prv_schedule_update(uint32_t delay, bool force_update) {
  bool success = new_timer_start(s_periodic_timer_id, delay, prv_update_callback,
                                 (void *)force_update, 0 /*flags*/);
  PBL_ASSERTN(success);
}

void battery_state_force_update(void) { prv_schedule_update(0, true); }

void battery_state_init(void) {
  int ret;
  struct nrf_fuel_gauge_init_parameters parameters = {0};
  struct nrf_fuel_gauge_runtime_parameters runtime_parameters = {0};
  BatteryConstants constants;

  parameters.model = &prv_battery_model;

  ret = battery_get_constants(&constants);
  PBL_ASSERTN(ret == 0);

  parameters.v0 = (float)constants.v_mv / 1000.0f;
  parameters.i0 = (float)FAKE_CURRENT_UA / 1000000.0f;
  parameters.t0 = (float)constants.t_mc / 1000.0f;

  s_last_voltage_mv = constants.v_mv;

  prv_ref_time = rtc_get_ticks();

#ifndef RECOVERY_FW
  {
    uint8_t saved_state[nrf_fuel_gauge_state_size];
    if (prv_load_state(saved_state, sizeof(saved_state))) {
      parameters.state = saved_state;
      ret = nrf_fuel_gauge_init(&parameters, NULL);
      if (ret == 0) {
        PBL_LOG_INFO("Fuel gauge state restored from flash");
        goto init_done;
      }
      PBL_LOG_WRN("Fuel gauge init with saved state failed (%d), reinitializing", ret);
      parameters.state = NULL;
    }
  }
#endif

  ret = nrf_fuel_gauge_init(&parameters, NULL);
  PBL_ASSERTN(ret == 0);

#ifndef RECOVERY_FW
init_done:
#endif

  ret = nrf_fuel_gauge_ext_state_update(
      NRF_FUEL_GAUGE_EXT_STATE_INFO_CHARGE_CURRENT_LIMIT,
      &(union nrf_fuel_gauge_ext_state_info_data){
          .charge_current_limit = (float)NPM1300_CONFIG.chg_current_ma / 1000.0f});
  PBL_ASSERTN(ret == 0);

  ret = nrf_fuel_gauge_ext_state_update(
      NRF_FUEL_GAUGE_EXT_STATE_INFO_TERM_CURRENT,
      &(union nrf_fuel_gauge_ext_state_info_data){
          .charge_term_current =
              (float)(NPM1300_CONFIG.chg_current_ma * NPM1300_CONFIG.term_current_pct / 100U) /
              1000.0f});
  PBL_ASSERTN(ret == 0);

  runtime_parameters.a = NAN_F;
  runtime_parameters.b = NAN_F;
  runtime_parameters.c = NAN_F;
  runtime_parameters.d = NAN_F;
  runtime_parameters.discard_positive_deltaz = true;

  nrf_fuel_gauge_param_adjust(&runtime_parameters);

  ret = battery_charge_status_get(&s_last_chg_status);
  PBL_ASSERTN(ret == 0);

  prv_charge_status_inform(s_last_chg_status);

  s_last_battery_charge_state.is_plugged = battery_is_usb_connected_impl();
  s_last_battery_charge_state.is_charging = s_last_battery_charge_state.is_plugged &&
                                            !(s_last_chg_status == BatteryChargeStatusComplete ||
                                              s_last_chg_status == BatteryChargeStatusUnknown);

  s_periodic_timer_id = new_timer_create();

  battery_state_force_update();

  static RegularTimerInfo battery_regular_timer = {
    .cb = prv_callback_from_regular_timer
  };
  regular_timer_add_multisecond_callback(&battery_regular_timer, BATTERY_SAMPLE_RATE_S);
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
TimerID battery_state_get_periodic_timer_id(void) { return s_periodic_timer_id; }

uint16_t battery_state_get_voltage(void) { return (uint16_t)s_last_voltage_mv; }

int32_t battery_state_get_temperature(void) { return s_last_temp_mc; }

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
void analytics_external_collect_battery(void) {
  // This should not be called for an hour after bootup
  int32_t d_mv;
  uint8_t d_pct;

  d_mv = s_last_voltage_mv - s_analytics_last_voltage_mv;
  analytics_set(ANALYTICS_DEVICE_METRIC_BATTERY_VOLTAGE, s_last_voltage_mv, AnalyticsClient_System);
  analytics_set(ANALYTICS_DEVICE_METRIC_BATTERY_VOLTAGE_DELTA, d_mv, AnalyticsClient_System);
  s_analytics_last_voltage_mv = s_last_voltage_mv;

  d_pct = s_last_battery_charge_state.pct - s_analytics_last_pct;
  analytics_set(ANALYTICS_DEVICE_METRIC_BATTERY_PERCENT_DELTA, d_pct, AnalyticsClient_System);
  analytics_set(ANALYTICS_DEVICE_METRIC_BATTERY_PERCENT, s_last_battery_charge_state.pct,
                AnalyticsClient_System);
  s_analytics_last_pct = s_last_battery_charge_state.pct;
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
