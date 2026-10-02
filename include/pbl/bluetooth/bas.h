/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup bluetooth_bas Battery Service
 * @ingroup bluetooth
 * @brief GATT Battery Service.
 * @{
 */

/**
 * @brief Update the battery level of the Battery Service.
 *
 * Notifies the subscribed connected devices.
 *
 * @param percent Battery level, 0 to 100.
 */
void pbl_bt_bas_handle_update(uint8_t percent);

/** @} */
