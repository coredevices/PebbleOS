/* SPDX-CopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/kernel_heap.h"
#include "memfault/components.h"
#include "drivers/imu/lsm6dso/lsm6dso.h"
#include "services/common/battery/battery_state.h"
#include "util/heap.h"
#include "services/common/analytics/analytics_metric_table.h"
#include "services/common/analytics/analytics_external.h"
#include "applib/app_message/app_message_internal.h"
#include "shell/normal/watchface.h"
#include "shell/normal/watchface_metrics.h"
#include "process_management/app_install_manager.h"

int memfault_platform_get_stateofcharge(sMfltPlatformBatterySoc *soc) {
  BatteryChargeState chargestate = battery_get_charge_state();

  *soc = (sMfltPlatformBatterySoc){
      .soc = chargestate.charge_percent,
      .discharging = !chargestate.is_charging,
  };

  return 0;
}

// Forward curated Pebble analytics metrics to Memfault (~70 total).
// Only forwards the subset of metrics we've defined in memfault_metrics_heartbeat_config.def
void memfault_metric_set_device_from_pebble_analytics(AnalyticsMetric metric, int64_t val) {
  switch (metric) {
    // System Health & Stability
    case ANALYTICS_DEVICE_METRIC_SYSTEM_CRASH_CODE:
      MEMFAULT_METRIC_SET_UNSIGNED(system_crash_code, val);
      break;
    case ANALYTICS_DEVICE_METRIC_SYSTEM_CRASH_LR:
      MEMFAULT_METRIC_SET_UNSIGNED(system_crash_lr, val);
      break;
    case ANALYTICS_DEVICE_METRIC_DEVICE_UP_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(device_up_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_KERNEL_HEAP_MIN_HEADROOM_BYTES:
      MEMFAULT_METRIC_SET_UNSIGNED(kernel_heap_min_headroom_bytes, val);
      break;
    case ANALYTICS_DEVICE_METRIC_STACK_FREE_KERNEL_MAIN:
      MEMFAULT_METRIC_SET_UNSIGNED(stack_free_kernel_main, val);
      break;
    case ANALYTICS_DEVICE_METRIC_STACK_FREE_KERNEL_BACKGROUND:
      MEMFAULT_METRIC_SET_UNSIGNED(stack_free_kernel_background, val);
      break;
    case ANALYTICS_DEVICE_METRIC_STACK_FREE_NEWTIMERS:
      MEMFAULT_METRIC_SET_UNSIGNED(stack_free_newtimers, val);
      break;
    case ANALYTICS_DEVICE_METRIC_PFS_SPACE_FREE_KB:
      MEMFAULT_METRIC_SET_UNSIGNED(pfs_space_free_kb, val);
      break;
    case ANALYTICS_DEVICE_METRIC_APP_CRASHED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(app_crashed_count, val);
      break;

    // Battery & Power
    case ANALYTICS_DEVICE_METRIC_BATTERY_VOLTAGE:
      MEMFAULT_METRIC_SET_UNSIGNED(battery_voltage, val);
      break;
    case ANALYTICS_DEVICE_METRIC_BATTERY_VOLTAGE_DELTA:
      MEMFAULT_METRIC_SET_SIGNED(battery_voltage_delta, val);
      break;
    case ANALYTICS_DEVICE_METRIC_BATTERY_CHARGE_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(battery_charge_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_BATTERY_PLUGGED_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(battery_plugged_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_CPU_RUNNING_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(cpu_running_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_CPU_STOP_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(cpu_stop_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_CPU_SLEEP_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(cpu_sleep_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_BACKLIGHT_ON_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(backlight_on_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_BACKLIGHT_ON_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(backlight_on_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_VIBRATOR_ON_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(vibrator_on_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_VIBRATOR_ON_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(vibrator_on_count, val);
      break;

    // User Interaction (2 metrics)
    case ANALYTICS_DEVICE_METRIC_BUTTON_PRESSED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(button_pressed_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_STATIONARY_TIME_MINUTES:
      MEMFAULT_METRIC_SET_UNSIGNED(stationary_time_minutes, val);
      break;

    // Accelerometer Health & Activity
    case ANALYTICS_DEVICE_METRIC_ACCEL_SAMPLE_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(accel_sample_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_ACCEL_SHAKE_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(accel_shake_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_ACCEL_DOUBLE_TAP_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(accel_double_tap_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_ACCEL_PEEK_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(accel_peek_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_ACCEL_XYZ_DELTA:
      MEMFAULT_METRIC_SET_UNSIGNED(accel_xyz_delta, val);
      break;

    // Hardware I/O Health
    case ANALYTICS_DEVICE_METRIC_I2C_ERROR_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(i2c_error_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_I2C_MAX_TRANSFER_DURATION_TICKS:
      MEMFAULT_METRIC_SET_UNSIGNED(i2c_max_transfer_duration_ticks, val);
      break;
    case ANALYTICS_DEVICE_METRIC_FLASH_READ_BYTES_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(flash_read_bytes_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_FLASH_WRITE_BYTES_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(flash_write_bytes_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_FLASH_ERASE_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(flash_erase_count, val);
      break;

    // Phone Call Handling
    case ANALYTICS_DEVICE_METRIC_PHONE_CALL_INCOMING_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(phone_call_incoming_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_PHONE_CALL_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(phone_call_time, val);
      break;

    // Notifications
    case ANALYTICS_DEVICE_METRIC_NOTIFICATION_RECEIVED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(notification_received_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_NOTIFICATION_RECEIVED_DND_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(notification_received_dnd_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_NOTIFICATION_DISMISSED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(notification_dismissed_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_NOTIFICATION_CLOSED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(notification_closed_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_NOTIFICATION_BYTE_IN_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(notification_byte_in_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_NOTIFICATION_ANCS_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(notification_ancs_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_NOTIFICATION_ANCS_PARSE_ERROR_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(notification_ancs_parse_error_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_NOTIFICATION_ANCS_SMS_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(notification_ancs_sms_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_REMINDER_RECEIVED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(reminder_received_count, val);
      break;

    // App Usage
    case ANALYTICS_DEVICE_METRIC_APP_ROCKY_LAUNCH_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(app_rocky_launch_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_APP_ROCKY_CRASHED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(app_rocky_crashed_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_APP_USER_LAUNCH_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(app_user_launch_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_APP_QUICK_LAUNCH_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(app_quick_launch_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_APP_THROTTLED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(app_throttled_count, val);
      break;
    case ANALYTICS_DEVICE_METRIC_APP_NOTIFIED_DISCONNECTED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(app_notified_disconnected_count, val);
      break;

    // User Preferences
    case ANALYTICS_DEVICE_METRIC_SETTING_BACKLIGHT:
      MEMFAULT_METRIC_SET_UNSIGNED(setting_backlight, val);
      break;
    case ANALYTICS_DEVICE_METRIC_SETTING_SHAKE_TO_LIGHT:
      MEMFAULT_METRIC_SET_UNSIGNED(setting_shake_to_light, val);
      break;
    case ANALYTICS_DEVICE_METRIC_SETTING_BACKLIGHT_INTENSITY_PCT:
      MEMFAULT_METRIC_SET_UNSIGNED(setting_backlight_intensity_pct, val);
      break;
    case ANALYTICS_DEVICE_METRIC_SETTING_BACKLIGHT_TIMEOUT_SEC:
      MEMFAULT_METRIC_SET_UNSIGNED(setting_backlight_timeout_sec, val);
      break;
    case ANALYTICS_DEVICE_METRIC_SETTING_VIBRATION_STRENGTH:
      MEMFAULT_METRIC_SET_UNSIGNED(setting_vibration_strength, val);
      break;

    // Display & UI
    case ANALYTICS_DEVICE_METRIC_WATCH_ONLY_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(watch_only_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_DISPLAY_UPDATES_PER_HOUR:
      MEMFAULT_METRIC_SET_UNSIGNED(display_updates_per_hour, val);
      break;

    // Health/HRM
    case ANALYTICS_DEVICE_METRIC_HRM_ON_TIME:
      MEMFAULT_METRIC_SET_UNSIGNED(hrm_on_time, val);
      break;
    case ANALYTICS_DEVICE_METRIC_ALARM_SOUNDED_COUNT:
      MEMFAULT_METRIC_SET_UNSIGNED(alarm_sounded_count, val);
      break;

    default:
      // This metric is not in our curated list, don't forward to Memfault
      break;
  }
}

// Record some metrics.
void memfault_metrics_heartbeat_collect_data(void) {
  // Kernel heap usage
  Heap *kernel_heap = kernel_heap_get();
  const uint32_t kernel_heap_size = heap_size(kernel_heap);
  const uint32_t kernel_heap_max_used = kernel_heap->high_water_mark;
  // kernel_heap_pct is a percentage with 2 decimal places of precision
  // (i.e. 10000 = 100.00%)
  const uint32_t kernel_heap_pct = (kernel_heap_max_used * 10000) / kernel_heap_size;

  MEMFAULT_LOG_INFO("Heap Usage: %lu/%lu (%lu.%02lu%%)\n", kernel_heap_max_used, kernel_heap_size,
                    kernel_heap_pct / 100, kernel_heap_pct % 100);

  MEMFAULT_METRIC_SET_UNSIGNED(memory_pct_max, kernel_heap_pct);

  // AppMessage metrics
  MEMFAULT_METRIC_SET_UNSIGNED(app_message_sent_count, app_message_outbox_get_sent_count());
  MEMFAULT_METRIC_SET_UNSIGNED(app_message_received_count, app_message_inbox_get_received_count());
  
  // Active watchface name and usage time
  AppInstallId watchface_id = watchface_get_default_install_id();
  AppInstallEntry watchface_entry;
  if (app_install_get_entry_for_install_id(watchface_id, &watchface_entry)) {
    MEMFAULT_METRIC_SET_STRING(active_watchface_name, watchface_entry.name);
  } else {
    MEMFAULT_METRIC_SET_STRING(active_watchface_name, "Unknown");
  }
  MEMFAULT_METRIC_SET_UNSIGNED(watchface_total_time_secs, watchface_metrics_get_current_time());

  // Update Pebble analytics and forward curated metrics to Memfault
  analytics_external_update();
}
