/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_touch_touch_nav_service Touch navigation
 * @ingroup services_touch
 * @brief Enabling and disabling system touch navigation.
 * @{
 */

/**
 * @brief Run the touch navigation enable or disable transaction.
 *
 * Coordinates the kernel and app navigation dispatchers and the system sensor hold in the order
 * required by touch_nav_transaction_apply(), and subscribes an already running app when
 * enabling. The pref is persisted by the shell before this is called; this flips the runtime
 * gate and updates the subscriptions and hold.
 *
 * @param enable true to turn touch navigation on, false to turn it off.
 */
void touch_nav_set_enabled(bool enable);

/**
 * @brief Notify that the master "Touch" pref changed without changing the effective state.
 *
 * Called when the "Touch Navigation" sub-pref is off. Re-evaluates the running app, since an app
 * that opted in follows the master pref alone.
 */
void touch_nav_master_changed(void);

/** @} */
