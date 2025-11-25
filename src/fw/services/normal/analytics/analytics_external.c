/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bluetooth/analytics.h"
#include "services/common/analytics/analytics_external.h"

void analytics_external_update(void) {
  analytics_external_collect_battery();
  analytics_external_collect_accel_xyz_delta();
  analytics_external_collect_app_cpu_stats();
  analytics_external_collect_app_flash_read_stats();
  analytics_external_collect_cpu_stats();
  analytics_external_collect_stop_inhibitor_stats(rtc_get_ticks());
  analytics_external_collect_chip_specific_parameters();
  analytics_external_collect_bt_pairing_info();
  analytics_external_collect_ble_parameters();
  analytics_external_collect_ble_pairing_info();
  analytics_external_collect_system_flash_statistics();
  analytics_external_collect_backlight_settings();
  analytics_external_collect_notification_settings();
  analytics_external_collect_system_theme_settings();
  analytics_external_collect_ancs_info();
  analytics_external_collect_dls_stats();
  analytics_external_collect_i2c_stats();
  analytics_external_collect_stack_free();
  analytics_external_collect_alerts_preferences();
  analytics_external_collect_timeline_pin_stats();
#if PLATFORM_SPALDING
  analytics_external_collect_display_offset();
#endif
  analytics_external_collect_pfs_stats();
  analytics_external_collect_bt_chip_heartbeat();
  analytics_external_collect_kernel_heap_stats();
  analytics_external_collect_accel_samples_received();
}
