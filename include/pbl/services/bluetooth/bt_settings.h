/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup services_bluetooth_bt_settings Bluetooth settings
 * @ingroup services_bluetooth
 * @brief Persistent Bluetooth settings of the firmware, for firmware with settings storage.
 * @{
 */

/**
 * @brief Get the airplane mode setting.
 *
 * @return True if airplane mode is on; false by default.
 */
bool bt_settings_get_airplane_mode(void);

/**
 * @brief Store the airplane mode setting.
 *
 * @param enable True to turn airplane mode on.
 */
void bt_settings_set_airplane_mode(bool enable);

/** @} */
