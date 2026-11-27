/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mfg_test_aging.h"

#include "apps/prf/mfg_test_result.h"
#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/vibes.h"
#include "applib/ui/window.h"
#include "board/board.h"
#include "console/console_internal.h"
#include "drivers/accel.h"
#include "drivers/ambient_light.h"
#include "drivers/audio.h"
#include "drivers/battery.h"
#include "drivers/backlight.h"
#include "drivers/mag.h"
#include "drivers/vibe.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "pbl/services/battery/battery_state.h"
#include "pbl/services/bluetooth/bluetooth_ctl.h"
#include "pbl/services/light.h"
#include "pbl/services/idle_watchdog.h"
#include "system/logging.h"
#include "util/time/time.h"

#include <stdio.h>

#define STATUS_STRING_LEN 240
#define RECORD_INTERVAL_SEC 60
#define COMPONENT_TEST_DURATION_SEC 10
#define COMPONENT_BACKLIGHT_DURATION_SEC 2
#define COMPONENT_VIBE_DURATION_SEC 1

// Cycling and charge phase parameters
#define CYCLE_DURATION_SEC (3 * 3600)
#define CHARGE_TIMEOUT_SEC (90 * 60)
#define CHARGE_COMPLETE_MIN_PERCENT 99
#define TEMP_MIN_MC 15000                         // 15.0C
#define TEMP_MAX_MC 35000                         // 35.0C

// Drain, settle and idle phase parameters
#define DRAIN_TARGET_PERCENT 70
#define SETTLE_DURATION_SEC (2 * 3600)
#define IDLE_DURATION_SEC (8 * 3600)
#define IDLE_MAX_DROP_PERCENT 3

#ifdef CONFIG_SPEAKER
static const int16_t sine_wave_4k[] = {
  0, 32767, 0, -32768, 0, 32767, 0, -32768,
  0, 32767, 0, -32768, 0, 32767, 0, -32768,
};
#endif

typedef enum {
  AgingStateWaitPlug = 0,
  AgingStateCycling,
  AgingStateCharging,
  AgingStateWaitUnplug,
  AgingStateDraining,
  AgingStateSettling,
  AgingStateIdle,
  AgingStatePass,
  AgingStateFail,
} AgingState;

// Component sub-states for cycling
typedef enum {
  ComponentAccel = 0,
#ifdef CONFIG_MAG
  ComponentMag,
#endif
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  ComponentBacklightWhite,
  ComponentBacklightRed,
  ComponentBacklightGreen,
  ComponentBacklightBlue,
#else
  ComponentBacklight,
#endif
#ifdef CONFIG_SPEAKER
  ComponentAudio,
#endif
  ComponentALS,
  ComponentVibe,
  NumComponents,
} ComponentState;

typedef struct {
  Window window;

  TextLayer title;
  TextLayer status;
  char status_string[STATUS_STRING_LEN];

  AgingState state;
  ComponentState component;
  uint32_t component_elapsed_sec;

  uint32_t phase_elapsed_sec;
  uint32_t cycle_count;

  // Battery state at start of idle phase, used for the drop check
  uint8_t initial_percent;
  int32_t initial_voltage_mv;
  uint32_t initial_soc_cpct;

  // Battery charge drawn in the current phase, from the PMIC current
  int64_t phase_uas;
  uint32_t record_elapsed_sec;

  uint32_t drain_sec;
  int32_t drain_uah;
  int32_t idle_uah;
  int32_t idle_mean_ua;

  char fail_reason[64];

#ifdef CONFIG_SPEAKER
  bool audio_playing;
#endif
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  uint32_t saved_backlight_color;
#endif
} AppData;

#ifdef CONFIG_SPEAKER
static void prv_audio_trans_handler(uint32_t *free_size) {
  uint32_t available_size = *free_size;
  while (available_size > sizeof(sine_wave_4k)) {
    available_size = audio_write(AUDIO, (void *)&sine_wave_4k[0], sizeof(sine_wave_4k));
  }
}
#endif

static void prv_format_time(char *buffer, size_t size, uint32_t seconds) {
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;
  sniprintf(buffer, size, "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, hours, minutes, secs);
}

static void prv_cleanup_component(AppData *data) {
#ifdef CONFIG_MAG
  if (data->component == ComponentMag) {
    mag_release();
  }
#endif
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  if (data->component >= ComponentBacklightWhite &&
      data->component <= ComponentBacklightBlue) {
    backlight_set_color(data->saved_backlight_color);
    light_enable(false);
  }
#else
  if (data->component == ComponentBacklight) {
    light_enable(false);
  }
#endif
#ifdef CONFIG_SPEAKER
  if (data->audio_playing) {
    audio_stop(AUDIO);
    data->audio_playing = false;
  }
#endif
}

static void prv_drain_light_enable(AppData *data, bool enable) {
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  backlight_set_color(enable ? 0xFFFFFF : data->saved_backlight_color);
#endif
  light_enable(enable);
}

static void prv_start_component(AppData *data) {
#ifdef CONFIG_MAG
  if (data->component == ComponentMag) {
    mag_start_sampling();
  }
#endif
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
  if (data->component >= ComponentBacklightWhite &&
      data->component <= ComponentBacklightBlue) {
    light_enable(true);
  }
#else
  if (data->component == ComponentBacklight) {
    light_enable(true);
  }
#endif
}

static uint32_t prv_component_duration(ComponentState comp) {
  switch (comp) {
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
    case ComponentBacklightWhite:
    case ComponentBacklightRed:
    case ComponentBacklightGreen:
    case ComponentBacklightBlue:
#else
    case ComponentBacklight:
#endif
      return COMPONENT_BACKLIGHT_DURATION_SEC;
    case ComponentVibe:
      return COMPONENT_VIBE_DURATION_SEC;
    default:
      return COMPONENT_TEST_DURATION_SEC;
  }
}

static void prv_advance_component(AppData *data) {
  prv_cleanup_component(data);

  data->component_elapsed_sec = 0;
  data->component++;

  if (data->component >= NumComponents) {
    data->component = ComponentAccel;
    data->cycle_count++;
  }

  prv_start_component(data);
}

static const char *const s_phase_names[] = {
  [AgingStateWaitPlug] = "waitplug", [AgingStateCycling] = "cycle",
  [AgingStateCharging] = "charge",   [AgingStateWaitUnplug] = "waitunplug",
  [AgingStateDraining] = "drain",    [AgingStateSettling] = "settle",
  [AgingStateIdle] = "idle",         [AgingStatePass] = "pass",
  [AgingStateFail] = "fail",
};

static void prv_log_battery(const AppData *data, const char *what, const BatteryConstants *bc,
                            const BatteryChargeState *cs) {
  uint32_t soc = battery_state_get_soc_cpct();
  PBL_LOG_INFO("Aging %s %s t=%" PRIu32 " V=%" PRId32 " I=%" PRId32 " T=%" PRId32,
               s_phase_names[data->state], what, data->phase_elapsed_sec, bc->v_mv, bc->i_ua,
               bc->t_mc);
  PBL_LOG_INFO("Aging %s %s soc=%" PRIu32 ".%02" PRIu32 " plg=%d chg=%d q=%" PRId32 "uAh",
               s_phase_names[data->state], what, soc / 100, soc % 100, cs->is_plugged,
               cs->is_charging, (int32_t)(data->phase_uas / 3600));
}

// Account the battery current over the last dt seconds, and log a record
// once a minute.
static void prv_account(AppData *data, const BatteryConstants *bc, const BatteryChargeState *cs,
                        uint32_t dt) {
  data->phase_uas += (int64_t)bc->i_ua * dt;
  data->record_elapsed_sec += dt;
  if (data->record_elapsed_sec >= RECORD_INTERVAL_SEC) {
    data->record_elapsed_sec = 0;
    prv_log_battery(data, "rec", bc, cs);
  }
}

static void prv_set_phase(AppData *data, AgingState state) {
  BatteryConstants bc = {0};
  battery_get_constants(&bc);
  BatteryChargeState cs = battery_get_charge_state();
  int32_t uah = (int32_t)(data->phase_uas / 3600);

  prv_log_battery(data, "end", &bc, &cs);
  if (data->state == AgingStateDraining) {
    data->drain_sec = data->phase_elapsed_sec;
    data->drain_uah = uah;
  } else if (data->state == AgingStateIdle) {
    data->idle_uah = uah;
    data->idle_mean_ua =
        data->phase_elapsed_sec ? (int32_t)(data->phase_uas / data->phase_elapsed_sec) : 0;
  }

  data->state = state;
  data->phase_elapsed_sec = 0;
  data->phase_uas = 0;
  data->record_elapsed_sec = 0;
  prv_log_battery(data, "start", &bc, &cs);
}

static void prv_format_summary(const AppData *data, char *buf, size_t size) {
  uint32_t soc = battery_state_get_soc_cpct();
  sniprintf(buf, size,
            "Drain %" PRIu32 "min %" PRId32 ".%02" PRId32 "mAh\n"
            "Idle %" PRId32 "uA %" PRId32 ".%02" PRId32 "mAh\n"
            "SoC %" PRIu32 ".%02" PRIu32 "->%" PRIu32 ".%02" PRIu32,
            data->drain_sec / 60, data->drain_uah / 1000, (data->drain_uah % 1000) / 10,
            data->idle_mean_ua, data->idle_uah / 1000, (data->idle_uah % 1000) / 10,
            data->initial_soc_cpct / 100, data->initial_soc_cpct % 100, soc / 100, soc % 100);
}

static void prv_report_result(bool passed) {
  // Always report into the FINISHED bucket: aging is launched outside the
  // per-mode test menu, so we can't rely on whatever mode happened to be
  // active last.
  mfg_test_result_set_mode(MFG_TEST_MODE_FINISHED);
  mfg_test_result_report(MfgTestId_Aging, passed, 0);
}

static void prv_enter_fail(AppData *data, const char *reason) {
  if (data->state == AgingStateDraining) {
    prv_drain_light_enable(data, false);
  } else if (data->state == AgingStateCycling) {
    prv_cleanup_component(data);
  }
  battery_set_charge_enable(true);
  prv_set_phase(data, AgingStateFail);
  sniprintf(data->fail_reason, sizeof(data->fail_reason), "%s", reason);
  PBL_LOG_ERR("Aging test FAIL: %s", reason);
  prv_report_result(false);
}

static const char *prv_charge_status_str(const BatteryChargeState *cs) {
  if (!cs->is_charging) {
    return "Idle";
  }
  BatteryChargeStatus charge_status;
  battery_charge_status_get(&charge_status);
  switch (charge_status) {
    case BatteryChargeStatusComplete: return "Cmpl";
    case BatteryChargeStatusTrickle:  return "Trk";
    case BatteryChargeStatusCC:       return "CC";
    case BatteryChargeStatusCV:       return "CV";
    default:                          return "?";
  }
}

static void prv_run_component_display(AppData *data) {
  char time_str[16];
  prv_format_time(time_str, sizeof(time_str), data->phase_elapsed_sec);

  uint32_t remaining = CYCLE_DURATION_SEC - data->phase_elapsed_sec;
  char rem_str[16];
  prv_format_time(rem_str, sizeof(rem_str), remaining);

  const char *comp_name = "?";
  char comp_detail[48] = "";
  switch (data->component) {
    case ComponentAccel: {
      AccelDriverSample sample;
      accel_peek(&sample);
      comp_name = "Accel";
      sniprintf(comp_detail, sizeof(comp_detail),
                "X:%" PRIi16 " Y:%" PRIi16 " Z:%" PRIi16,
                sample.x, sample.y, sample.z);
      break;
    }
#ifdef CONFIG_MAG
    case ComponentMag: {
      MagData mag_sample;
      mag_read_data(&mag_sample);
      comp_name = "Mag";
      sniprintf(comp_detail, sizeof(comp_detail),
                "X:%" PRIi16 " Y:%" PRIi16 " Z:%" PRIi16,
                mag_sample.x, mag_sample.y, mag_sample.z);
      break;
    }
#endif
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
    case ComponentBacklightWhite:
      backlight_set_color(0xD0D0D0);
      comp_name = "BL White";
      break;
    case ComponentBacklightRed:
      backlight_set_color(0xFF0000);
      comp_name = "BL Red";
      break;
    case ComponentBacklightGreen:
      backlight_set_color(0x00FF00);
      comp_name = "BL Green";
      break;
    case ComponentBacklightBlue:
      backlight_set_color(0x0000FF);
      comp_name = "BL Blue";
      break;
#else
    case ComponentBacklight:
      comp_name = "Backlight";
      break;
#endif
#ifdef CONFIG_SPEAKER
    case ComponentAudio:
      comp_name = "Audio";
      if (!data->audio_playing) {
        audio_start(AUDIO, prv_audio_trans_handler);
        audio_set_volume(AUDIO, 100);
        data->audio_playing = true;
      }
      break;
#endif
    case ComponentALS: {
      uint32_t level = ambient_light_get_light_level();
      comp_name = "ALS";
      sniprintf(comp_detail, sizeof(comp_detail), "Level: %" PRIu32, level);
      break;
    }
    case ComponentVibe: {
      // Only pulse once at the start of the vibe phase
      if (data->component_elapsed_sec <= 1) {
        static const uint32_t vibe_durations[] = { 250 };
        static const uint32_t vibe_amplitudes[] = { 50 };
        VibePatternWithAmplitudes pat = {
          .durations = vibe_durations,
          .amplitudes = vibe_amplitudes,
          .num_segments = 1,
        };
        vibes_enqueue_custom_pattern_with_amplitudes(pat);
      }
      comp_name = "Vibe";
      break;
    }
    default:
      break;
  }

  BatteryConstants bc;
  battery_get_constants(&bc);
  BatteryChargeState cs = battery_get_charge_state();

  int8_t temp_c = (int8_t)(bc.t_mc / 1000);
  uint8_t temp_c_frac = ((bc.t_mc > 0 ? bc.t_mc : -bc.t_mc) % 1000) / 10;

  const char *chg_str = prv_charge_status_str(&cs);

  sniprintf(data->status_string, sizeof(data->status_string),
            "CYCLE [%s]\n"
            "Cycle: %" PRIu32 "\n"
            "Elapsed: %s\n"
            "Remain: %s\n"
            "%" PRId32 "mV %" PRIu8 "%% %s\n"
            "%" PRId8 ".%02" PRIu8 "C\n"
            "%s",
            comp_name, data->cycle_count,
            time_str, rem_str,
            bc.v_mv, cs.charge_percent, chg_str,
            temp_c, temp_c_frac,
            comp_detail);
}

static void prv_handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  switch (data->state) {
    case AgingStateWaitPlug: {
      BatteryChargeState cs = battery_get_charge_state();
      if (cs.is_plugged) {
        prv_set_phase(data, AgingStateCycling);
        // Cycle on external power only; the battery is charged afterwards.
        battery_set_charge_enable(false);
        data->component = ComponentAccel;
        data->component_elapsed_sec = 0;
        data->cycle_count = 1;
        prv_start_component(data);
        sniprintf(data->status_string, sizeof(data->status_string),
                  "CYCLE\nStarting...");
      } else {
        sniprintf(data->status_string, sizeof(data->status_string),
                  "PLUG CHARGER\n\nPlug the watch\nto start");
      }
      break;
    }

    case AgingStateCycling: {
      data->phase_elapsed_sec++;
      data->component_elapsed_sec++;

      BatteryConstants bc;
      battery_get_constants(&bc);
      BatteryChargeState cs = battery_get_charge_state();
      prv_account(data, &bc, &cs, 1);

      if (!cs.is_plugged) {
        prv_enter_fail(data, "Unplugged during\ncycling");
        break;
      }

      if (bc.t_mc < TEMP_MIN_MC || bc.t_mc > TEMP_MAX_MC) {
        prv_enter_fail(data, "Temperature out\nof range");
        break;
      }

      if (data->phase_elapsed_sec >= CYCLE_DURATION_SEC) {
        prv_cleanup_component(data);
        battery_set_charge_enable(true);
        prv_set_phase(data, AgingStateCharging);
        sniprintf(data->status_string, sizeof(data->status_string), "CHARGING\nStarting...");
        break;
      }

      if (data->component_elapsed_sec >= prv_component_duration(data->component)) {
        prv_advance_component(data);
      }

      prv_run_component_display(data);
      break;
    }

    case AgingStateCharging: {
      data->phase_elapsed_sec++;

      BatteryConstants bc;
      battery_get_constants(&bc);
      BatteryChargeState cs = battery_get_charge_state();
      prv_account(data, &bc, &cs, 1);

      if (!cs.is_plugged) {
        prv_enter_fail(data, "Unplugged during\ncharge");
        break;
      }

      if (bc.t_mc < TEMP_MIN_MC || bc.t_mc > TEMP_MAX_MC) {
        prv_enter_fail(data, "Temperature out\nof range");
        break;
      }

      // Full once the PMIC stops charging on its own at a near-full SoC.
      if (!cs.is_charging && cs.charge_percent >= CHARGE_COMPLETE_MIN_PERCENT) {
        prv_set_phase(data, AgingStateWaitUnplug);
        break;
      }

      if (data->phase_elapsed_sec >= CHARGE_TIMEOUT_SEC) {
        prv_enter_fail(data, "Charge timeout\n(90min)");
        break;
      }

      char time_str[16], rem_str[16];
      prv_format_time(time_str, sizeof(time_str), data->phase_elapsed_sec);
      prv_format_time(rem_str, sizeof(rem_str), CHARGE_TIMEOUT_SEC - data->phase_elapsed_sec);

      int8_t temp_c = (int8_t)(bc.t_mc / 1000);
      uint8_t temp_c_frac = ((bc.t_mc > 0 ? bc.t_mc : -bc.t_mc) % 1000) / 10;

      sniprintf(data->status_string, sizeof(data->status_string),
                "CHARGING\n"
                "Elapsed: %s\n"
                "Timeout: %s\n"
                "%" PRId32 "mV %" PRIu8 "%% %s\n"
                "%" PRId8 ".%02" PRIu8 "C",
                time_str, rem_str, bc.v_mv, cs.charge_percent, prv_charge_status_str(&cs),
                temp_c, temp_c_frac);
      break;
    }

    case AgingStateWaitUnplug: {
      BatteryChargeState cs = battery_get_charge_state();
      if (!cs.is_plugged) {
        // Near full the fuel gauge can sit at 99-100% for hours regardless of
        // consumption, so the idle drop is measured from a settled lower SoC.
        prv_set_phase(data, AgingStateDraining);
        prv_drain_light_enable(data, true);
        // The remaining phases only need occasional screen refreshes, so tick
        // once a minute to cut wake-ups and the test's own power draw.
        tick_timer_service_subscribe(MINUTE_UNIT, prv_handle_tick);
        sniprintf(data->status_string, sizeof(data->status_string), "DRAINING\nStarting...");
      } else {
        sniprintf(data->status_string, sizeof(data->status_string),
                  "UNPLUG WATCH\n\nCharged to %" PRIu8 "%%\n"
                  "Unplug to begin\ndrain",
                  cs.charge_percent);
      }
      break;
    }

    case AgingStateDraining: {
      data->phase_elapsed_sec += SECONDS_PER_MINUTE;

      BatteryConstants bc;
      battery_get_constants(&bc);
      BatteryChargeState cs = battery_get_charge_state();
      prv_account(data, &bc, &cs, SECONDS_PER_MINUTE);

      if (cs.is_plugged) {
        prv_enter_fail(data, "Plugged while\ndraining");
        break;
      }

      if (cs.charge_percent <= DRAIN_TARGET_PERCENT) {
        prv_drain_light_enable(data, false);
        prv_set_phase(data, AgingStateSettling);
        sniprintf(data->status_string, sizeof(data->status_string), "SETTLING\nStarting...");
        break;
      }

      char time_str[16];
      prv_format_time(time_str, sizeof(time_str), data->phase_elapsed_sec);

      int8_t temp_c = (int8_t)(bc.t_mc / 1000);
      uint8_t temp_c_frac = ((bc.t_mc > 0 ? bc.t_mc : -bc.t_mc) % 1000) / 10;

      sniprintf(data->status_string, sizeof(data->status_string),
                "DRAINING\nTime: %s\n\n"
                "%" PRId32 "mV %" PRIu8
                "%%\n"
                "%" PRId8 ".%02" PRIu8
                "C\n\n"
                "Target: %d%%",
                time_str, bc.v_mv, cs.charge_percent, temp_c, temp_c_frac, DRAIN_TARGET_PERCENT);
      break;
    }

    case AgingStateSettling: {
      data->phase_elapsed_sec += SECONDS_PER_MINUTE;

      BatteryConstants bc;
      battery_get_constants(&bc);
      BatteryChargeState cs = battery_get_charge_state();
      prv_account(data, &bc, &cs, SECONDS_PER_MINUTE);

      if (cs.is_plugged) {
        prv_enter_fail(data, "Plugged while\nsettling");
        break;
      }

      // The fuel gauge keeps converging for a while after the drain, so the
      // idle baseline is only taken once it has settled.
      if (data->phase_elapsed_sec >= SETTLE_DURATION_SEC) {
        prv_set_phase(data, AgingStateIdle);
        data->initial_percent = cs.charge_percent;
        data->initial_voltage_mv = bc.v_mv;
        data->initial_soc_cpct = battery_state_get_soc_cpct();
        sniprintf(data->status_string, sizeof(data->status_string), "IDLE\nStarting...");
        break;
      }

      char time_str[16], rem_str[16];
      prv_format_time(time_str, sizeof(time_str), data->phase_elapsed_sec);
      prv_format_time(rem_str, sizeof(rem_str), SETTLE_DURATION_SEC - data->phase_elapsed_sec);

      int8_t temp_c = (int8_t)(bc.t_mc / 1000);
      uint8_t temp_c_frac = ((bc.t_mc > 0 ? bc.t_mc : -bc.t_mc) % 1000) / 10;

      sniprintf(data->status_string, sizeof(data->status_string),
                "SETTLING\n"
                "Elapsed: %s\n"
                "Remain: %s\n"
                "%" PRId32 "mV %" PRIu8
                "%%\n"
                "%" PRId8 ".%02" PRIu8 "C",
                time_str, rem_str, bc.v_mv, cs.charge_percent, temp_c, temp_c_frac);
      break;
    }

    case AgingStateIdle: {
      data->phase_elapsed_sec += SECONDS_PER_MINUTE;

      BatteryConstants bc;
      battery_get_constants(&bc);
      BatteryChargeState cs = battery_get_charge_state();
      prv_account(data, &bc, &cs, SECONDS_PER_MINUTE);

      if (cs.is_plugged) {
        prv_enter_fail(data, "Plugged during\nidle test");
        break;
      }

      int8_t drop = (int8_t)data->initial_percent - (int8_t)cs.charge_percent;
      if (drop < 0) {
        drop = 0;
      }

      // Phase done?
      if (data->phase_elapsed_sec >= IDLE_DURATION_SEC) {
        int32_t drop_mv = data->initial_voltage_mv - bc.v_mv;
        PBL_LOG_INFO("Aging idle: SoC %u -> %u, %" PRId32 "mV -> %" PRId32 "mV",
                     (unsigned int)data->initial_percent, (unsigned int)cs.charge_percent,
                     data->initial_voltage_mv, bc.v_mv);

        if (drop > IDLE_MAX_DROP_PERCENT) {
          char reason[64];
          sniprintf(reason, sizeof(reason),
                    "Idle drop %" PRId8 "%%\n> %d%%",
                    drop, IDLE_MAX_DROP_PERCENT);
          prv_enter_fail(data, reason);
          break;
        }

        prv_set_phase(data, AgingStatePass);
        char summary[128];
        prv_format_summary(data, summary, sizeof(summary));
        sniprintf(data->status_string, sizeof(data->status_string),
                  "PASS\nDrop %" PRId8 "%% %" PRId32 "mV\n%s", drop, drop_mv, summary);
        prv_report_result(true);
        tick_timer_service_unsubscribe();
        break;
      }

      char time_str[16], rem_str[16];
      prv_format_time(time_str, sizeof(time_str), data->phase_elapsed_sec);
      prv_format_time(rem_str, sizeof(rem_str),
                      IDLE_DURATION_SEC - data->phase_elapsed_sec);

      int8_t temp_c = (int8_t)(bc.t_mc / 1000);
      uint8_t temp_c_frac = ((bc.t_mc > 0 ? bc.t_mc : -bc.t_mc) % 1000) / 10;

      sniprintf(data->status_string, sizeof(data->status_string),
                "IDLE\n"
                "Elapsed: %s\n"
                "Remain: %s\n"
                "%" PRId32 "mV %" PRIu8 "%%\n"
                "%" PRId8 ".%02" PRIu8 "C\n"
                "Start:%" PRIu8 "%% Drop:%" PRId8 "/%d%%",
                time_str, rem_str,
                bc.v_mv, cs.charge_percent,
                temp_c, temp_c_frac,
                data->initial_percent, drop, IDLE_MAX_DROP_PERCENT);
      break;
    }

    case AgingStatePass:
    case AgingStateFail:
      return;
  }

  if (data->state == AgingStateFail) {
    char summary[128];
    prv_format_summary(data, summary, sizeof(summary));
    sniprintf(data->status_string, sizeof(data->status_string), "FAIL\n%s\n%s",
              data->fail_reason, summary);
  }

  text_layer_set_text(&data->status, data->status_string);
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  if (data->state == AgingStateDraining) {
    prv_drain_light_enable(data, false);
  } else if (data->state == AgingStateCycling) {
    prv_cleanup_component(data);
  }
  battery_set_charge_enable(true);

  tick_timer_service_unsubscribe();

  serial_console_set_rx_enabled(true);
  bt_ctl_set_enabled(true);
  prf_idle_watchdog_start();

  app_window_stack_pop(true);
}

static void prv_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData){
    .state = AgingStateWaitPlug,
#ifdef CONFIG_SPEAKER
    .audio_playing = false,
#endif
#ifdef CONFIG_BACKLIGHT_HAS_COLOR
    .saved_backlight_color = backlight_get_color(),
#endif
  };

  app_state_set_user_data(data);

  // Disable power consumers and idle watchdog for the entire test
  serial_console_set_rx_enabled(false);
  bt_ctl_set_enabled(false);
  prf_idle_watchdog_stop();

  // Make sure charging is enabled at start of test in case a previous run
  // left it disabled.
  battery_set_charge_enable(true);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, prv_config_provider);

  Layer *window_layer = &window->layer;
  GRect bounds = window_layer->bounds;

  TextLayer *title = &data->title;
  const int16_t title_y = PBL_IF_ROUND_ELSE(10, 0);
  text_layer_init(title, &GRect(0, title_y, bounds.size.w, 24));
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(title, GTextAlignmentCenter);
  text_layer_set_text(title, "AGING TEST");
  layer_add_child(window_layer, &title->layer);

  TextLayer *status = &data->status;
  const int16_t status_y = PBL_IF_ROUND_ELSE(40, 25);
  const int16_t status_x = PBL_IF_ROUND_ELSE(15, 5);
  text_layer_init(status, &GRect(status_x, status_y, bounds.size.w - (status_x * 2),
                                 bounds.size.h - status_y));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(status, PBL_IF_ROUND_ELSE(GTextAlignmentCenter,
                                                           GTextAlignmentLeft));
  layer_add_child(window_layer, &status->layer);

  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_tick);

  app_window_stack_push(window, true);
}

static void s_main(void) {
  prv_handle_init();

  app_event_loop();
}

const PebbleProcessMd *mfg_test_aging_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: 12345678-ABCD-EF01-2345-6789ABCDEF01
    .common.uuid = { 0x12, 0x34, 0x56, 0x78, 0xAB, 0xCD, 0xEF, 0x01,
                     0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01 },
    .name = "MfgTestAging",
  };
  return (const PebbleProcessMd *)&s_app_info;
}
