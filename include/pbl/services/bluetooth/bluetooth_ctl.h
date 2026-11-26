/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_bluetooth Bluetooth services
 * @ingroup services
 * @brief Firmware-side Bluetooth management on top of the Bluetooth stack.
 *
 * Covers starting and stopping the stack, the local identity and
 * address, pairability, and the GATT services the firmware implements (battery, device
 * information).
 *
 * The stack runs while it is enabled (by the runlevel system) and airplane mode is off:
 *
 * @code{.c}
 * bt_ctl_set_airplane_mode_async(false);
 *
 * // Once the stack is up, be discoverable and pairable for a minute.
 * if (bt_ctl_is_bluetooth_running()) {
 *   bt_pairability_use_for_period(60);
 * }
 * @endcode
 */

/**
 * @defgroup services_bluetooth_bluetooth_ctl Bluetooth control
 * @ingroup services_bluetooth
 * @brief Start, stop and reset the Bluetooth stack.
 * @{
 */

/**
 * @brief Initialize Bluetooth control, loading the persisted airplane mode setting.
 *
 * Must be called before any setter.
 */
void bt_ctl_init(void);

/**
 * @brief Get the airplane mode setting.
 *
 * @return True if airplane mode is on.
 */
bool bt_ctl_is_airplane_mode_on(void);

/**
 * @brief Check whether the stack is supposed to be running.
 *
 * It may not actually be running yet, e.g. while starting or resetting.
 *
 * @return True if enabled and not stopped by airplane mode.
 */
bool bt_ctl_is_bluetooth_active(void);

/**
 * @brief Check whether the stack is up and running.
 *
 * @return True if running.
 */
bool bt_ctl_is_bluetooth_running(void);

/**
 * @brief Set and persist airplane mode.
 *
 * The stack is started or stopped later from SysWorkq.
 *
 * @param enabled True to turn airplane mode on.
 */
void bt_ctl_set_airplane_mode_async(bool enabled);

/**
 * @brief Enable or disable Bluetooth, as requested by the runlevel system.
 *
 * Starts or stops the stack synchronously as needed.
 *
 * @param enabled True to enable.
 */
void bt_ctl_set_enabled(bool enabled);

/**
 * @brief Restart the stack from SysWorkq.
 *
 * No-op if Bluetooth is not active.
 */
void bt_ctl_reset_bluetooth(void);

/** @} */
