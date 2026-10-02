/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_bluetooth_ble_bas Battery service
 * @ingroup services_bluetooth
 * @brief Feeds the battery level to the BLE Battery Service.
 * @{
 */

/**
 * @brief Start reporting the battery level, on KernelMain.
 */
void ble_bas_init(void);

/**
 * @brief Stop reporting the battery level, on KernelMain.
 */
void ble_bas_deinit(void);

/** @} */
