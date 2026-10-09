/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/battery/battery_state.h>
#include <pbl/services/new_timer/new_timer.h>

/**
 * @defgroup services_battery_battery_monitor Battery monitor
 * @ingroup services_battery
 * @brief Power state control in response to battery state changes.
 *
 * Enters low power mode below the board's low power threshold while discharging, and standby
 * when the battery stays critical (0 % and not charging) for 30 s, or 2 s right after boot.
 * Neither happens while plugged in, nor on QEMU outside recovery firmware.
 * @{
 */

/** @brief Initialize the monitor and battery state tracking. */
void battery_monitor_init(void);

/**
 * @brief Update the power state after a battery state change.
 *
 * Called on KernelMain for each @c PEBBLE_BATTERY_STATE_CHANGE_EVENT.
 *
 * @param state New battery state.
 */
void battery_monitor_handle_state_change_event(PreciseBatteryChargeState state);

/**
 * @brief Check whether the UI must be locked out because the battery is critical.
 *
 * @return true when the battery is critical, or was low at boot while unplugged.
 */
bool battery_monitor_critical_lockout(void);

/**
 * @brief Get the standby timer, for unit tests.
 *
 * @return Timer id.
 */
TimerID battery_monitor_get_standby_timer_id(void);

/** @} */
