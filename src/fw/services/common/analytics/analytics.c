/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

extern void analytics_external_collect_battery(void);
extern void analytics_external_collect_cpu_stats(void);
extern void analytics_external_collect_stack_free(void);
extern void analytics_external_collect_pfs_stats(void);
extern void analytics_external_collect_kernel_heap_stats(void);
extern void analytics_external_collect_backlight_stats(void);
extern void analytics_external_collect_vibe_stats(void);
extern void analytics_external_collect_settings(void);

void analytics_external_update(void) {
  analytics_external_collect_battery();
  analytics_external_collect_cpu_stats();
  analytics_external_collect_stack_free();
  analytics_external_collect_pfs_stats();
  analytics_external_collect_kernel_heap_stats();
  analytics_external_collect_backlight_stats();
  analytics_external_collect_vibe_stats();
  analytics_external_collect_settings();
}