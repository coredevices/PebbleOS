/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_bluetooth_battery_level Battery level
 * @ingroup services_bluetooth
 * @brief Feeds the battery level to the BLE Battery Service.
 * @{
 */

/**
 * @brief Start reporting the battery level, on KernelMain.
 */
void bt_battery_level_start(void);

/**
 * @brief Stop reporting the battery level, on KernelMain.
 */
void bt_battery_level_stop(void);

/** @} */
