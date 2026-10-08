/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_orientation_manager Orientation manager
 * @ingroup services
 * @brief Rotate display and input devices to match the display orientation preference.
 *
 * Applies the orientation to the display, buttons, accelerometer, magnetometer and touch.
 * Available with @c CONFIG_ORIENTATION_MANAGER.
 * @{
 */

#ifdef CONFIG_ORIENTATION_MANAGER
#include <shell/prefs.h>

/**
 * @brief Apply the orientation preference after it changed.
 *
 * Also asks the running app to redraw.
 */
void orientation_handle_prefs_changed(void);

/**
 * @brief Runlevel hook.
 *
 * @param on True to apply the orientation preference, false to force the default orientation.
 */
void orientation_manager_enable(bool on);
#endif

/** @} */
