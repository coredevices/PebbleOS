/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

extern void memfault_platform_boot_early(void);

void analytics_impl_init(void) {
  memfault_platform_boot_early();
}