/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <pbl/drivers/mcu_reboot_reason.h>

#ifdef CONFIG_WATCHDOG

void watchdog_init(void);
void watchdog_start(void);
void watchdog_stop(void);

void watchdog_feed(void);

bool watchdog_check_reset_flag(void);
McuRebootReason watchdog_clear_reset_flag(void);

//! Returns the cached value from the most recent watchdog_clear_reset_flag().
McuRebootReason watchdog_get_reset_flag(void);

#else

static inline void watchdog_init(void) {
}
static inline void watchdog_start(void) {
}
static inline void watchdog_stop(void) {
}
static inline void watchdog_feed(void) {
}
static inline bool watchdog_check_reset_flag(void) {
  return false;
}
static inline McuRebootReason watchdog_clear_reset_flag(void) {
  return (McuRebootReason){0};
}
static inline McuRebootReason watchdog_get_reset_flag(void) {
  return (McuRebootReason){0};
}

#endif
