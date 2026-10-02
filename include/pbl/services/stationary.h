/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup services_stationary Stationary mode
 * @ingroup services
 * @brief Low power mode entered when the watch is not moving.
 *
 * Once enabled, the accelerometer is sampled every minute. After 30 minutes without motion the
 * system switches to the stationary runlevel; motion or a button press switches it back.
 * Stationary mode only runs while the user setting allows it, the runlevel permits it and the
 * charger is disconnected.
 * @{
 */

/** @brief Initialize the service. */
void stationary_init(void);

/**
 * @brief Get the user setting.
 *
 * @return True if the user allows stationary mode.
 */
bool stationary_get_enabled(void);

/**
 * @brief Change the user setting.
 *
 * Disabling ends any stationary period and keeps the watch from entering stationary mode.
 *
 * @param enabled True to allow stationary mode.
 */
void stationary_set_enabled(bool enabled);

/**
 * @brief Runlevel hook.
 *
 * @param allow True if the current runlevel allows stationary mode.
 */
void stationary_run_level_enable(bool allow);

/**
 * @brief Leave stationary mode if active.
 *
 * Call before doing something that likely needs user interaction, such as an alarm. Must be
 * called on KernelMain.
 */
void stationary_wake_up(void);

/** @brief Handle a charger connection change. Called on KernelMain. */
void stationary_handle_battery_connection_change_event(void);

/** @} */
