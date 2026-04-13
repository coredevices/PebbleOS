/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "heartbeat_v70.h"

#include <string.h>

#include "drivers/rtc.h"
#include "services/common/battery/battery_state.h"
#include "services/normal/data_logging/data_logging_service.h"
#include "services/normal/vibes/vibe_intensity.h"
#include "shell/prefs.h"

// ---------------------------------------------------------------------------
// v70 packed struct — must match eng-dash blob-parser.ts exactly
// ---------------------------------------------------------------------------

#define V70_BLOB_VERSION 70
#define V70_FORMAT_DEVICE_HEARTBEAT 0x00

typedef struct __attribute__((packed)) {
  uint8_t  format_type;            // 0x00
  uint16_t version;                // 70
} V70BlobHeader;

typedef struct __attribute__((packed)) {
  uint32_t timestamp;              // unix epoch UTC
  uint32_t duration_ms;
  uint8_t  battery_soc_pct;
  int32_t  battery_soc_pct_drop;
  uint16_t battery_voltage;
  int16_t  battery_voltage_delta;
  uint32_t battery_charge_time_ms;
  uint32_t battery_plugged_time_ms;
  uint32_t cpu_running_pct;        // scaled x100
  uint32_t cpu_sleep0_pct;         // scaled x100
  uint32_t cpu_sleep1_pct;         // scaled x100
  uint32_t cpu_sleep2_pct;         // scaled x100
  uint32_t backlight_on_time_ms;
  uint8_t  backlight_avg_intensity_pct;
  uint32_t vibrator_on_time_ms;
  uint8_t  vibrator_avg_strength_pct;
  uint32_t hrm_on_time_ms;
  uint32_t accel_sample_count;
  uint32_t accel_shake_count;
  uint32_t accel_double_tap_count;
  uint32_t accel_peek_count;
  uint32_t button_pressed_count;
  uint32_t notification_received_count;
  uint32_t notification_received_dnd_count;
  uint32_t phone_call_incoming_count;
  uint32_t phone_call_time_ms;
  uint32_t memory_pct_max;
  uint32_t stack_free_kernel_main_bytes;
  uint32_t stack_free_kernel_background_bytes;
  uint32_t stack_free_newtimers_bytes;
  uint32_t pfs_space_free_kb;
  uint32_t flash_spi_write_bytes;
  uint32_t flash_spi_erase_bytes;
  uint32_t low_power_time_ms;
  uint32_t stationary_time_ms;
  uint32_t watchface_time_ms;
  uint32_t connectivity_connected_time_ms;
  uint32_t ble_adv_short_intvl_time_ms;
  uint32_t ble_adv_long_intvl_time_ms;
  uint32_t ble_conn_itvl_min_time_ms;
  uint32_t ble_conn_itvl_mid_time_ms;
  uint32_t ble_conn_itvl_max_time_ms;
  uint32_t settings_health_tracking_enabled;
  uint32_t settings_health_hrm_enabled;
  uint32_t settings_health_hrm_measurement_interval;
  uint32_t settings_health_hrm_activity_tracking_enabled;
  uint32_t app_message_sent_count;
  uint32_t app_message_received_count;
  int32_t  utc_offset_s;
  uint8_t  setting_backlight;
  uint8_t  setting_backlight_intensity_pct;
  uint8_t  setting_backlight_timeout_sec;
  uint8_t  setting_shake_to_light;
  uint8_t  setting_vibration_strength;
  char     watchface_name[32];
  char     watchface_uuid[39];
} V70DeviceHeartbeat;

_Static_assert(sizeof(V70DeviceHeartbeat) == 259,
               "V70 body size must be exactly 259 bytes");
_Static_assert(sizeof(V70BlobHeader) == 3,
               "V70 header size must be exactly 3 bytes");

// ---------------------------------------------------------------------------
// Shadow store
//
// Two categories of metrics:
//   "accumulator" — counters (ADD) and timers: reset to zero after each send
//   "snapshot"    — SET values: persist across sends until next SET call
// ---------------------------------------------------------------------------

static int64_t  s_shadow[V70_METRIC_COUNT];
static RtcTicks s_timer_start[V70_METRIC_COUNT];
static char     s_watchface_name[32];
static char     s_watchface_uuid[39];

// Track which metrics were set via SET (not ADD/TIMER) so we preserve them
static bool s_is_snapshot[V70_METRIC_COUNT];

void v70_shadow_set_unsigned(V70MetricId id, uint64_t value) {
  if (id < V70_METRIC_COUNT) {
    s_shadow[id] = (int64_t)value;
    s_is_snapshot[id] = true;
  }
}

void v70_shadow_set_signed(V70MetricId id, int64_t value) {
  if (id < V70_METRIC_COUNT) {
    s_shadow[id] = value;
    s_is_snapshot[id] = true;
  }
}

void v70_shadow_add(V70MetricId id, int64_t amount) {
  if (id < V70_METRIC_COUNT) {
    s_shadow[id] += amount;
    // ADD metrics are accumulators — not marked as snapshot
  }
}

void v70_shadow_timer_start(V70MetricId id) {
  if (id < V70_METRIC_COUNT) {
    s_timer_start[id] = rtc_get_ticks();
  }
}

void v70_shadow_timer_stop(V70MetricId id) {
  if (id < V70_METRIC_COUNT && s_timer_start[id] != 0) {
    RtcTicks elapsed = rtc_get_ticks() - s_timer_start[id];
    uint32_t elapsed_ms = (uint32_t)((elapsed * 1000) / RTC_TICKS_HZ);
    s_shadow[id] += elapsed_ms;
    s_timer_start[id] = 0;
  }
}

void v70_shadow_set_string(V70MetricId id, const char *value) {
  if (id == V70_METRIC_watchface_name) {
    strncpy(s_watchface_name, value ? value : "", sizeof(s_watchface_name) - 1);
    s_watchface_name[sizeof(s_watchface_name) - 1] = '\0';
  } else if (id == V70_METRIC_watchface_uuid) {
    strncpy(s_watchface_uuid, value ? value : "", sizeof(s_watchface_uuid) - 1);
    s_watchface_uuid[sizeof(s_watchface_uuid) - 1] = '\0';
  }
}

// ---------------------------------------------------------------------------
// Connectivity refcount — tracks overlapping system sessions
// ---------------------------------------------------------------------------

static int s_connectivity_refcount;

void v70_connectivity_connected(void) {
  if (s_connectivity_refcount++ == 0) {
    v70_shadow_timer_start(V70_METRIC_connectivity_connected_time_ms);
  }
}

void v70_connectivity_disconnected(void) {
  if (s_connectivity_refcount > 0 && --s_connectivity_refcount == 0) {
    v70_shadow_timer_stop(V70_METRIC_connectivity_connected_time_ms);
  }
}

// ---------------------------------------------------------------------------
// DLS session & timing
// ---------------------------------------------------------------------------

static DataLoggingSession *s_dls_session;
static bool s_dls_initialized;
static RtcTicks s_last_send_ticks;
static uint8_t s_last_battery_pct;

void v70_heartbeat_init(void) {
  // Don't create DLS session here — DLS may not be initialized yet at boot.
  // Defer to first v70_heartbeat_send() call.
  s_last_send_ticks = rtc_get_ticks();
  s_last_battery_pct = battery_get_charge_state().charge_percent;
}

static void prv_ensure_dls_session(void) {
  if (s_dls_initialized) {
    return;
  }
  if (!dls_initialized()) {
    return;
  }
  s_dls_session = dls_create(
      DlsSystemTagAnalyticsDeviceHeartbeat,
      DATA_LOGGING_BYTE_ARRAY,
      sizeof(V70BlobHeader) + sizeof(V70DeviceHeartbeat),
      true,   // buffered
      false,  // don't resume
      NULL    // system UUID
  );
  s_dls_initialized = true;
}

// ---------------------------------------------------------------------------
// Collect the 8 "old" metrics from subsystem getters
// ---------------------------------------------------------------------------

static void prv_collect_old_metrics(V70DeviceHeartbeat *hb) {
  // Battery SOC
  BatteryChargeState batt = battery_get_charge_state();
  hb->battery_soc_pct = batt.charge_percent;
  hb->battery_soc_pct_drop = (int32_t)s_last_battery_pct - (int32_t)batt.charge_percent;
  s_last_battery_pct = batt.charge_percent;

  // Backlight settings
  hb->setting_backlight = (uint8_t)backlight_get_timeout_ms() > 0 ?
      (backlight_is_motion_enabled() ? 2 : 1) : 0;
  hb->setting_backlight_intensity_pct = backlight_get_intensity_percent();
  hb->setting_backlight_timeout_sec = (uint8_t)(backlight_get_timeout_ms() / 1000);
  hb->setting_shake_to_light = backlight_is_motion_enabled() ? 1 : 0;

  // Vibration strength
  hb->setting_vibration_strength = (uint8_t)vibe_intensity_get();
}

// ---------------------------------------------------------------------------
// Assemble and send the v70 heartbeat
// ---------------------------------------------------------------------------

// Convenience: read a shadow value as uint32, clamped
#define SHADOW_U32(metric) ((uint32_t)s_shadow[V70_METRIC_##metric])
#define SHADOW_I32(metric) ((int32_t)s_shadow[V70_METRIC_##metric])
#define SHADOW_U16(metric) ((uint16_t)s_shadow[V70_METRIC_##metric])
#define SHADOW_I16(metric) ((int16_t)s_shadow[V70_METRIC_##metric])

void v70_heartbeat_send(void) {
  // Lazily create DLS session on first send (fix #1: DLS not ready at boot)
  prv_ensure_dls_session();
  if (!s_dls_session) {
    return;
  }

  // Snapshot running timers: capture elapsed time without stopping them
  for (int i = 0; i < V70_METRIC_COUNT; i++) {
    if (s_timer_start[i] != 0) {
      RtcTicks now = rtc_get_ticks();
      RtcTicks elapsed = now - s_timer_start[i];
      uint32_t elapsed_ms = (uint32_t)((elapsed * 1000) / RTC_TICKS_HZ);
      s_shadow[i] += elapsed_ms;
      // Restart the timer for the next period
      s_timer_start[i] = now;
    }
  }

  // Build the blob
  uint8_t buf[sizeof(V70BlobHeader) + sizeof(V70DeviceHeartbeat)];
  V70BlobHeader *header = (V70BlobHeader *)buf;
  V70DeviceHeartbeat *hb = (V70DeviceHeartbeat *)(buf + sizeof(V70BlobHeader));

  memset(buf, 0, sizeof(buf));

  // Header
  header->format_type = V70_FORMAT_DEVICE_HEARTBEAT;
  header->version = V70_BLOB_VERSION;

  // Timestamp and duration
  hb->timestamp = (uint32_t)rtc_get_time();
  RtcTicks now = rtc_get_ticks();
  hb->duration_ms = (uint32_t)(((now - s_last_send_ticks) * 1000) / RTC_TICKS_HZ);
  s_last_send_ticks = now;

  // Shadowed Memfault metrics
  hb->battery_voltage           = SHADOW_U16(battery_voltage);
  hb->battery_voltage_delta     = SHADOW_I16(battery_voltage_delta);
  hb->battery_charge_time_ms    = SHADOW_U32(battery_charge_time_ms);
  hb->battery_plugged_time_ms   = SHADOW_U32(battery_plugged_time_ms);
  hb->cpu_running_pct           = SHADOW_U32(cpu_running_pct);
  hb->cpu_sleep0_pct            = SHADOW_U32(cpu_sleep0_pct);
  hb->cpu_sleep1_pct            = SHADOW_U32(cpu_sleep1_pct);
  hb->cpu_sleep2_pct            = SHADOW_U32(cpu_sleep2_pct);
  hb->backlight_on_time_ms      = SHADOW_U32(backlight_on_time_ms);
  hb->backlight_avg_intensity_pct = (uint8_t)s_shadow[V70_METRIC_backlight_avg_intensity_pct];
  hb->vibrator_on_time_ms       = SHADOW_U32(vibrator_on_time_ms);
  hb->vibrator_avg_strength_pct = (uint8_t)s_shadow[V70_METRIC_vibrator_avg_strength_pct];
  hb->hrm_on_time_ms            = SHADOW_U32(hrm_on_time_ms);
  hb->accel_sample_count        = SHADOW_U32(accel_sample_count);
  hb->accel_shake_count         = SHADOW_U32(accel_shake_count);
  hb->accel_double_tap_count    = SHADOW_U32(accel_double_tap_count);
  hb->accel_peek_count          = SHADOW_U32(accel_peek_count);
  hb->button_pressed_count      = SHADOW_U32(button_pressed_count);
  hb->notification_received_count     = SHADOW_U32(notification_received_count);
  hb->notification_received_dnd_count = SHADOW_U32(notification_received_dnd_count);
  hb->phone_call_incoming_count = SHADOW_U32(phone_call_incoming_count);
  hb->phone_call_time_ms        = SHADOW_U32(phone_call_time_ms);
  hb->memory_pct_max            = SHADOW_U32(memory_pct_max);
  hb->stack_free_kernel_main_bytes       = SHADOW_U32(stack_free_kernel_main_bytes);
  hb->stack_free_kernel_background_bytes = SHADOW_U32(stack_free_kernel_background_bytes);
  hb->stack_free_newtimers_bytes         = SHADOW_U32(stack_free_newtimers_bytes);
  hb->pfs_space_free_kb         = SHADOW_U32(pfs_space_free_kb);
  hb->flash_spi_write_bytes     = SHADOW_U32(flash_spi_write_bytes);
  hb->flash_spi_erase_bytes     = SHADOW_U32(flash_spi_erase_bytes);
  hb->low_power_time_ms         = SHADOW_U32(low_power_time_ms);
  hb->stationary_time_ms        = SHADOW_U32(stationary_time_ms);
  hb->watchface_time_ms         = SHADOW_U32(watchface_time_ms);
  hb->connectivity_connected_time_ms = SHADOW_U32(connectivity_connected_time_ms);
  hb->ble_adv_short_intvl_time_ms = SHADOW_U32(ble_adv_short_intvl_time_ms);
  hb->ble_adv_long_intvl_time_ms  = SHADOW_U32(ble_adv_long_intvl_time_ms);
  hb->ble_conn_itvl_min_time_ms   = SHADOW_U32(ble_conn_itvl_min_time_ms);
  hb->ble_conn_itvl_mid_time_ms   = SHADOW_U32(ble_conn_itvl_mid_time_ms);
  hb->ble_conn_itvl_max_time_ms   = SHADOW_U32(ble_conn_itvl_max_time_ms);
  hb->settings_health_tracking_enabled            = SHADOW_U32(settings_health_tracking_enabled);
  hb->settings_health_hrm_enabled                 = SHADOW_U32(settings_health_hrm_enabled);
  hb->settings_health_hrm_measurement_interval    = SHADOW_U32(settings_health_hrm_measurement_interval);
  hb->settings_health_hrm_activity_tracking_enabled = SHADOW_U32(settings_health_hrm_activity_tracking_enabled);
  hb->app_message_sent_count    = SHADOW_U32(app_message_sent_count);
  hb->app_message_received_count = SHADOW_U32(app_message_received_count);
  hb->utc_offset_s              = SHADOW_I32(utc_offset_s);

  // Strings (these persist — not cleared after send)
  memcpy(hb->watchface_name, s_watchface_name, sizeof(hb->watchface_name));
  memcpy(hb->watchface_uuid, s_watchface_uuid, sizeof(hb->watchface_uuid));

  // The 8 "old" metrics read directly from subsystems
  prv_collect_old_metrics(hb);

  // Send via data logging
  dls_log(s_dls_session, buf, 1);

  // Reset only accumulator metrics (ADD and TIMER), preserve snapshot (SET) values
  // Fix #2: snapshot metrics like utc_offset_s, watchface strings persist
  for (int i = 0; i < V70_METRIC_COUNT; i++) {
    if (!s_is_snapshot[i]) {
      s_shadow[i] = 0;
    }
  }
  // Note: s_watchface_name/uuid are NOT cleared — they persist until next SET_STRING
  // Note: s_timer_start is NOT reset — running timers continue accumulating
}
