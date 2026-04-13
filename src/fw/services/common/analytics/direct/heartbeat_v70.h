/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// v70 metric IDs for the shadow store.
// Names match the Memfault key names so PBL_ANALYTICS_* macros can use
// token pasting: V70_METRIC_##key_name
typedef enum {
  // Memfault-shadowed metrics (accumulated via PBL_ANALYTICS_* macros)
  V70_METRIC_battery_voltage = 0,
  V70_METRIC_battery_voltage_delta,
  V70_METRIC_battery_charge_time_ms,
  V70_METRIC_battery_plugged_time_ms,
  V70_METRIC_cpu_running_pct,
  V70_METRIC_cpu_sleep0_pct,
  V70_METRIC_cpu_sleep1_pct,
  V70_METRIC_cpu_sleep2_pct,
  V70_METRIC_backlight_on_time_ms,
  V70_METRIC_backlight_avg_intensity_pct,
  V70_METRIC_vibrator_on_time_ms,
  V70_METRIC_vibrator_avg_strength_pct,
  V70_METRIC_hrm_on_time_ms,
  V70_METRIC_accel_sample_count,
  V70_METRIC_accel_shake_count,
  V70_METRIC_accel_double_tap_count,
  V70_METRIC_accel_peek_count,
  V70_METRIC_button_pressed_count,
  V70_METRIC_notification_received_count,
  V70_METRIC_notification_received_dnd_count,
  V70_METRIC_phone_call_incoming_count,
  V70_METRIC_phone_call_time_ms,
  V70_METRIC_memory_pct_max,
  V70_METRIC_stack_free_kernel_main_bytes,
  V70_METRIC_stack_free_kernel_background_bytes,
  V70_METRIC_stack_free_newtimers_bytes,
  V70_METRIC_pfs_space_free_kb,
  V70_METRIC_flash_spi_write_bytes,
  V70_METRIC_flash_spi_erase_bytes,
  V70_METRIC_low_power_time_ms,
  V70_METRIC_stationary_time_ms,
  V70_METRIC_watchface_time_ms,
  V70_METRIC_connectivity_connected_time_ms,
  V70_METRIC_ble_adv_short_intvl_time_ms,
  V70_METRIC_ble_adv_long_intvl_time_ms,
  V70_METRIC_ble_conn_itvl_min_time_ms,
  V70_METRIC_ble_conn_itvl_mid_time_ms,
  V70_METRIC_ble_conn_itvl_max_time_ms,
  V70_METRIC_settings_health_tracking_enabled,
  V70_METRIC_settings_health_hrm_enabled,
  V70_METRIC_settings_health_hrm_measurement_interval,
  V70_METRIC_settings_health_hrm_activity_tracking_enabled,
  V70_METRIC_app_message_sent_count,
  V70_METRIC_app_message_received_count,
  V70_METRIC_utc_offset_s,
  V70_METRIC_COUNT, // = 45

  // String metrics — handled specially, not in the int64 array
  V70_METRIC_watchface_name = V70_METRIC_COUNT,
  V70_METRIC_watchface_uuid,
} V70MetricId;

// Shadow store API
void v70_shadow_set_unsigned(V70MetricId id, uint64_t value);
void v70_shadow_set_signed(V70MetricId id, int64_t value);
void v70_shadow_add(V70MetricId id, int64_t amount);
void v70_shadow_timer_start(V70MetricId id);
void v70_shadow_timer_stop(V70MetricId id);
void v70_shadow_set_string(V70MetricId id, const char *value);

// Connectivity refcount — handles overlapping system sessions
void v70_connectivity_connected(void);
void v70_connectivity_disconnected(void);

// Lifecycle
void v70_heartbeat_init(void);
void v70_heartbeat_send(void);
