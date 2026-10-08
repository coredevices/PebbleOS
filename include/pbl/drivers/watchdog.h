/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/drivers/mcu_reboot_reason.h>

/**
 * @defgroup drivers_watchdog Watchdog
 * @ingroup drivers
 * @brief Hardware watchdog and reset cause.
 *
 * Without @c CONFIG_WATCHDOG, all functions are no-ops and no reset cause is reported.
 *
 * @code{.c}
 * watchdog_init();
 * watchdog_start();
 * // periodically
 * watchdog_feed();
 * @endcode
 * @{
 */

#ifdef CONFIG_WATCHDOG

/** @brief Configure the watchdog, without starting it. */
void watchdog_init(void);
/** @brief Start the watchdog. */
void watchdog_start(void);
/** @brief Stop the watchdog. */
void watchdog_stop(void);

/** @brief Restart the watchdog countdown. */
void watchdog_feed(void);

/**
 * @brief Check whether the last reset was caused by the watchdog.
 *
 * @return true after a watchdog reset.
 */
bool watchdog_check_reset_flag(void);
/**
 * @brief Read and clear the hardware reset cause.
 *
 * @return Reset cause, also cached for watchdog_get_reset_flag().
 */
McuRebootReason watchdog_clear_reset_flag(void);

/**
 * @brief Get the reset cause read by the last watchdog_clear_reset_flag().
 *
 * @return Cached reset cause.
 */
McuRebootReason watchdog_get_reset_flag(void);

#else

/** @cond INTERNAL_HIDDEN */
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
/** @endcond */

#endif

/** @} */
